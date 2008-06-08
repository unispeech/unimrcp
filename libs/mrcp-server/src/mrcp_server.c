/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <apr_hash.h>
#include <apr_tables.h>
#include "mrcp_server.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_server_connection.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mpf_user.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define MRCP_SESSION_ID_HEX_STRING_LENGTH 16

/** MRCP server */
struct mrcp_server_t {
	/** Main message processing task */
	apt_consumer_task_t       *task;

	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** Media processing engine */
	mpf_engine_t              *media_engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** Signaling agent */
	mrcp_sig_agent_t          *signaling_agent;
	/** Connection agent */
	mrcp_connection_agent_t   *connection_agent;
	/** Connection task message pool */
	apt_task_msg_pool_t       *connection_msg_pool;
	
	/** MRCP sessions table */
	apr_hash_t                *session_table;

	/** Memory pool */
	apr_pool_t                *pool;
};

typedef struct mrcp_server_session_t mrcp_server_session_t;
struct mrcp_server_session_t {
	/** Session base */
	mrcp_session_t base;

	/** Media context */
	mpf_context_t *context;

	/** Media termination array */
	apr_array_header_t *terminations;
	/** MRCP control channel array */
	apr_array_header_t *channels;

	/** In-progress offer */
	mrcp_session_descriptor_t *offer;
	/** In-progres answer */
	mrcp_session_descriptor_t *answer;
};

typedef struct mrcp_channel_t mrcp_channel_t;
struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t        *pool;
	/** MRCP resource */
	mrcp_resource_t   *resource;
	/** MRCP session entire channel belongs to (added for fast reverse search) */
	mrcp_session_t    *session;
	/** MRCP connection */
	mrcp_connection_t *connection;

	/** Media termination */
	mpf_termination_t *termination;
};


typedef enum {
	MRCP_SERVER_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_SERVER_CONNECTION_TASK_MSG,
	MRCP_SERVER_MEDIA_TASK_MSG
} mrcp_server_task_msg_type_e ;


/* Signaling agent interface */
typedef enum {
	SIG_AGENT_TASK_MSG_OFFER,
	SIG_AGENT_TASK_MSG_TERMINATE
} sig_agent_task_msg_type_e ;

typedef struct sig_agent_message_t sig_agent_message_t;
struct sig_agent_message_t {
	mrcp_session_t            *session;
	mrcp_session_descriptor_t *descriptor;
};

static apt_bool_t mrcp_server_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_server_session_terminate(mrcp_session_t *session);

static const mrcp_session_method_vtable_t session_method_vtable = {
	mrcp_server_session_offer,
	NULL, /* answer */
	mrcp_server_session_terminate
};


/* Connection agent interface */
typedef enum {
	CONNECTION_AGENT_TASK_MSG_ANSWER,
	CONNECTION_AGENT_TASK_MSG_TERMINATE
} connection_agent_task_msg_type_e ;

typedef struct connection_agent_message_t connection_agent_message_t;
struct connection_agent_message_t {
	mrcp_connection_agent_t   *agent;
	mrcp_channel_t            *channel;
	mrcp_connection_t         *connection;
	mrcp_control_descriptor_t *descriptor;
};

static apt_bool_t mrcp_server_channel_answer(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor);

static const mrcp_connection_event_vtable_t connection_method_vtable = {
	mrcp_server_channel_answer
};

/* Media interface */
static apt_bool_t mpf_request_send(mrcp_server_t *server, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor);


/* Task interface */
static void mrcp_server_on_start_complete(apt_task_t *task);
static void mrcp_server_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static mrcp_session_t* mrcp_server_session_create();
static mrcp_channel_t* mrcp_server_channel_create(mrcp_session_t *session, apt_str_t *resource_name);


/** Create MRCP server instance */
MRCP_DECLARE(mrcp_server_t*) mrcp_server_create()
{
	mrcp_server_t *server;
	apr_pool_t *pool;
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}

	apt_log(APT_PRIO_NOTICE,"Create MRCP Server");
	server = apr_palloc(pool,sizeof(mrcp_server_t));
	server->pool = pool;
	server->resource_factory = NULL;
	server->media_engine = NULL;
	server->rtp_termination_factory = NULL;
	server->signaling_agent = NULL;
	server->connection_agent = NULL;
	server->connection_msg_pool = NULL;
	server->session_table = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = mrcp_server_msg_process;
	vtable.on_start_complete = mrcp_server_on_start_complete;
	vtable.on_terminate_complete = mrcp_server_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);

	server->task = apt_consumer_task_create(server, &vtable, msg_pool, pool);
	if(!server->task) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Server Task");
		return NULL;
	}
	
	return server;
}

/** Start message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_server_start(mrcp_server_t *server)
{
	apt_task_t *task;
	if(!server->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(server->task);
	apt_log(APT_PRIO_INFO,"Start Server Task");
	server->session_table = apr_hash_make(server->pool);
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Start Server Task");
		return FALSE;
	}
	return TRUE;
}

/** Shutdown message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_server_shutdown(mrcp_server_t *server)
{
	apt_task_t *task;
	if(!server->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(server->task);
	apt_log(APT_PRIO_INFO,"Shutdown Server Task");
	if(apt_task_terminate(task,TRUE) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Shutdown Server Task");
		return FALSE;
	}
	server->session_table = NULL;
	return TRUE;
}

/** Destroy MRCP server */
MRCP_DECLARE(apt_bool_t) mrcp_server_destroy(mrcp_server_t *server)
{
	apt_task_t *task;
	if(!server->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(server->task);
	apt_log(APT_PRIO_INFO,"Destroy Server Task");
	apt_task_destroy(task);

	apr_pool_destroy(server->pool);
	return TRUE;
}

/** Register MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_server_resource_factory_register(mrcp_server_t *server, mrcp_resource_factory_t *resource_factory)
{
	server->resource_factory = resource_factory;
	return TRUE;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_server_media_engine_register(mrcp_server_t *server, mpf_engine_t *media_engine)
{
	if(!media_engine) {
		return FALSE;
	}
	server->media_engine = media_engine;
	mpf_engine_task_msg_type_set(media_engine,MRCP_SERVER_MEDIA_TASK_MSG);
	if(server->task) {
		apt_task_t *media_task = mpf_task_get(media_engine);
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,media_task);
	}
	return TRUE;
}

/** Register RTP termination factory */
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_termination_factory_register(mrcp_server_t *server, mpf_termination_factory_t *rtp_termination_factory)
{
	if(!rtp_termination_factory) {
		return FALSE;
	}
	server->rtp_termination_factory = rtp_termination_factory;
	return TRUE;
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_server_signaling_agent_register(mrcp_server_t *server, mrcp_sig_agent_t *signaling_agent)
{
	if(!signaling_agent) {
		return FALSE;
	}
	signaling_agent->create_session = mrcp_server_session_create;
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(sig_agent_message_t),server->pool);
	server->signaling_agent = signaling_agent;
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_register(mrcp_server_t *server, mrcp_connection_agent_t *connection_agent)
{
	if(!connection_agent) {
		return FALSE;
	}
	mrcp_server_connection_agent_handler_set(connection_agent,server,&connection_method_vtable);
	server->connection_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_message_t),server->pool);
	server->connection_agent = connection_agent;
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_t *connection_task = mrcp_server_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
	return TRUE;
}

MRCP_DECLARE(apr_pool_t*) mrcp_server_memory_pool_get(mrcp_server_t *server)
{
	return server->pool;
}


static void mrcp_server_on_start_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Server Task Start");
}

static void mrcp_server_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Server Task Terminate");
}

static apt_bool_t mrcp_server_control_media_offer_process(mrcp_server_t *server, mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_channel_t *channel;
	mrcp_control_descriptor_t *control_descriptor;
	int i;
	int count = session->channels->nelts;
	if(count > descriptor->control_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of control channels [%d] > Number of control media in offer [%d]\n",
			count,descriptor->control_media_arr->nelts);
		count = descriptor->control_media_arr->nelts;
	}
	
	/* update existing control channels */
	for(i=0; i<count; i++) {
		/* get existing termination */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		/* send offer */
		mrcp_server_connection_agent_offer(server->connection_agent,channel,channel->connection,control_descriptor,channel->pool);
	}
	
	/* add new control channels */
	for(; i<descriptor->control_media_arr->nelts; i++) {
		mrcp_channel_t **slot;
		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		if(!control_descriptor) continue;

		/* create new MRCP channel instance */
		channel = mrcp_server_channel_create(&session->base,&control_descriptor->resource_name);
		/* add to channel array */
		slot = apr_array_push(session->channels);
		*slot = channel;

		/* send offer */
		mrcp_server_connection_agent_offer(server->connection_agent,channel,channel->connection,control_descriptor,channel->pool);
	}

	return TRUE;
}

static apt_bool_t mrcp_server_av_media_offer_process(mrcp_server_t *server, mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mpf_termination_t *termination;
	int i;
	int count = session->terminations->nelts;
	if(count > descriptor->audio_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of terminations [%d] > Number of audio media in offer [%d]\n",
			count,descriptor->audio_media_arr->nelts);
		count = descriptor->audio_media_arr->nelts;
	}
	
	/* update existing terminations */
	for(i=0; i<count; i++) {
		/* get existing termination */
		termination = *((mpf_termination_t**)session->terminations->elts + i);
		if(!termination) continue;

		/* send modify termination request */
		mpf_request_send(server,MPF_COMMAND_MODIFY,session->context,termination,NULL);
	}
	
	/* add new terminations */
	for(; i<descriptor->audio_media_arr->nelts; i++) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		mpf_termination_t **slot;
		/* create new RTP termination instance */
		termination = mpf_termination_create(server->rtp_termination_factory,session,session->base.pool);
		/* add to termination array */
		slot = apr_array_push(session->terminations);
		*slot = termination;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send add termination request (add to media context) */
		mpf_request_send(server,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor);
	}

	return TRUE;
}

static apt_bool_t mrcp_server_session_offer_process(mrcp_server_t *server, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_server_session_t *server_session = (mrcp_server_session_t*)session;
	apt_log(APT_PRIO_INFO,"Process Session Offer");
	if(server_session->offer) {
		/* last offer received is still in-progress, new offer is not allowed */
		apt_log(APT_PRIO_INFO,"Cannot Accept New Offer");
		return FALSE;
	}
	if(!session->id.length) {
		/* initial offer received, generate session id and add to session's table */
		apt_unique_id_generate(&session->id,MRCP_SESSION_ID_HEX_STRING_LENGTH,server->pool);
		apr_hash_set(server->session_table,session->id.buf,session->id.length,session);

		server_session->context = mpf_context_create(server_session,5,session->pool);
	}

	mrcp_server_control_media_offer_process(server,server_session,descriptor);
	mrcp_server_av_media_offer_process(server,server_session,descriptor);

	/* store last received offer */
	server_session->offer = descriptor;
	server_session->answer = mrcp_session_descriptor_create(session->pool);
	return TRUE;
}

static apt_bool_t mrcp_server_session_terminate_process(mrcp_server_t *server, mrcp_session_t *session)
{
	apt_log(APT_PRIO_INFO,"Process Session Terminate");
	apr_hash_set(server->session_table,session->id.buf,session->id.length,NULL);
	return TRUE;
}

static apt_bool_t mrcp_server_session_answer_is_ready(const mrcp_session_descriptor_t *offer, const mrcp_session_descriptor_t *answer)
{
	if(!offer || !answer) {
		return FALSE;
	}
	return	(offer->control_media_arr->nelts == answer->control_media_arr->nelts &&
			 offer->audio_media_arr->nelts == answer->audio_media_arr->nelts &&
			 offer->video_media_arr->nelts == answer->video_media_arr->nelts);
}

static apt_bool_t mrcp_server_session_answer_send(mrcp_server_t *server, mrcp_server_session_t *session)
{
	apt_bool_t status = mrcp_session_on_answer(&session->base,session->answer);
	session->offer = NULL;
	session->answer = NULL;
	return status;
}

static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);
	apt_log(APT_PRIO_DEBUG,"Process Message");
	switch(msg->type) {
		case MRCP_SERVER_SIGNALING_TASK_MSG:
		{
			const sig_agent_message_t *sig_message = (const sig_agent_message_t*)msg->data;
			switch(msg->sub_type) {
				case SIG_AGENT_TASK_MSG_OFFER:
					mrcp_server_session_offer_process(server,sig_message->session,sig_message->descriptor);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE:
					mrcp_server_session_terminate_process(server,sig_message->session);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_CONNECTION_TASK_MSG:
		{
			const connection_agent_message_t *connection_message = (const connection_agent_message_t*)msg->data;
			switch(msg->sub_type) {
				case CONNECTION_AGENT_TASK_MSG_ANSWER:
					apt_log(APT_PRIO_DEBUG,"On Control Channel Answer");
					if(connection_message->descriptor) {
						mrcp_server_session_t *session = (mrcp_server_session_t*)connection_message->channel->session;
						connection_message->channel->connection = connection_message->connection;
						
						mrcp_session_control_media_add(session->answer,connection_message->descriptor);

						if(mrcp_server_session_answer_is_ready(session->offer,session->answer) == TRUE) {
							mrcp_server_session_answer_send(server,session);
						}
					}
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_MEDIA_TASK_MSG:
		{
			mrcp_server_session_t *session = NULL;
			const mpf_message_t *mpf_message = (const mpf_message_t*) msg->data;
			if(mpf_message->termination) {
				session = mpf_termination_object_get(mpf_message->termination);
			}
			if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
				if(mpf_message->command_id == MPF_COMMAND_ADD) {
					apt_log(APT_PRIO_DEBUG,"On Add Termination");
					if(session && session->answer) {
						mpf_rtp_termination_descriptor_t *rtp_descriptor = mpf_message->descriptor;
						if(rtp_descriptor->audio.local) {
							mrcp_session_audio_media_add(session->answer,rtp_descriptor->audio.local);

							if(mrcp_server_session_answer_is_ready(session->offer,session->answer) == TRUE) {
								mrcp_server_session_answer_send(server,session);
							}
						}
					}
				}
				else if(mpf_message->command_id == MPF_COMMAND_MODIFY) {
					apt_log(APT_PRIO_DEBUG,"On Modify Termination");
				}
				else if(mpf_message->command_id == MPF_COMMAND_SUBTRACT) {
					apt_log(APT_PRIO_DEBUG,"On Subtract Termination");
				}
			}
			else if(mpf_message->message_type == MPF_MESSAGE_TYPE_EVENT) {
				apt_log(APT_PRIO_DEBUG,"Process MPF Event");
			}
			break;
		}
		default: 
			break;
	}
	return TRUE;
}


/* Signaling interface */
static mrcp_session_t* mrcp_server_session_create()
{
	mrcp_server_session_t *session = (mrcp_server_session_t*) mrcp_session_create(sizeof(mrcp_server_session_t)-sizeof(mrcp_session_t));
	session->base.method_vtable = &session_method_vtable;

	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mpf_termination_t*));
	session->channels = apr_array_make(session->base.pool,2,sizeof(void*));
	session->offer = NULL;
	session->answer = NULL;
	return &session->base;
}

static mrcp_channel_t* mrcp_server_channel_create(mrcp_session_t *session, apt_str_t *resource_name)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->session = session;
	channel->connection = NULL;
	channel->termination = NULL;
	channel->resource = NULL;
	return channel;
}

static apt_bool_t mrcp_server_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	sig_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_SERVER_SIGNALING_TASK_MSG;
	task_msg->sub_type = SIG_AGENT_TASK_MSG_OFFER;
	message = (sig_agent_message_t*) task_msg->data;
	message->session = session;
	message->descriptor = descriptor;

	apt_log(APT_PRIO_DEBUG,"Session Offer");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_server_session_terminate(mrcp_session_t *session)
{
	sig_agent_message_t *data;
	apt_task_msg_t *msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	msg->type = MRCP_SERVER_SIGNALING_TASK_MSG;
	msg->sub_type = SIG_AGENT_TASK_MSG_TERMINATE;
	data = (sig_agent_message_t*) msg->data;
	data->session = session;
	data->descriptor = NULL;

	apt_log(APT_PRIO_DEBUG,"Session Terminate");
	return apt_task_msg_parent_signal(session->signaling_agent->task,msg);
}


/* Connection interface */
static apt_bool_t mrcp_server_channel_answer(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor)
{
	mrcp_server_t *server = mrcp_server_connection_agent_object_get(agent);
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	connection_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->connection_msg_pool);
	task_msg->type = MRCP_SERVER_CONNECTION_TASK_MSG;
	task_msg->sub_type = CONNECTION_AGENT_TASK_MSG_ANSWER;
	message = (connection_agent_message_t*) task_msg->data;
	message->agent = agent;
	message->channel = handle;
	message->connection = connection;
	message->descriptor = descriptor;

	return apt_task_msg_signal(task,task_msg);
}


/* Media interface */
static apt_bool_t mpf_request_send(mrcp_server_t *server, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor)
{
	apt_task_t *media_task = mpf_task_get(server->media_engine);
	apt_task_msg_t *msg;
	mpf_message_t *mpf_message;
	msg = apt_task_msg_get(media_task);
	msg->type = TASK_MSG_USER;
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = termination;
	mpf_message->descriptor = descriptor;
	return apt_task_msg_signal(media_task,msg);
}
