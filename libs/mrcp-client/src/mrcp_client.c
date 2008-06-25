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
#include "mrcp_client.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_client_connection.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "mpf_user.h"
#include "apt_consumer_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

/** MRCP client */
struct mrcp_client_t {
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
	/** Application task message pool */
	apt_task_msg_pool_t       *application_msg_pool;
	
	/** MRCP sessions table */
	apr_hash_t                *session_table;

	/** Memory pool */
	apr_pool_t                *pool;
};

struct mrcp_application_t {
	void                      *obj;
	mrcp_version_e             version;
	mrcp_app_message_handler_f handler;
	mrcp_client_t             *client;
};

typedef struct mrcp_client_session_t mrcp_client_session_t;
struct mrcp_client_session_t {
	/** Session base */
	mrcp_session_t             base;
	/** Application session belongs to */
	mrcp_application_t        *application;

	/** Media context */
	mpf_context_t             *context;

	/** RTP termination array (mrcp_termination_slot_t) */
	apr_array_header_t        *terminations;
	/** MRCP control channel array (mrcp_channel_t*) */
	apr_array_header_t        *channels;

	/** In-progress offer */
	mrcp_session_descriptor_t *offer;
	/** In-progress answer */
	mrcp_session_descriptor_t *answer;

	/** MRCP application active request */
	const mrcp_app_message_t  *active_request;
	/** MRCP application request queue */
	apt_obj_list_t            *request_queue;

	apr_size_t                 offer_flag_count;
	apr_size_t                 answer_flag_count;
	apr_size_t                 terminate_flag_count;
};

struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t        *pool;
	/** MRCP resource identifier */
	mrcp_resource_id   resource_id;
	/** MRCP resource */
	mrcp_resource_t   *resource;
	/** MRCP session entire channel belongs to (added for fast reverse search) */
	mrcp_session_t    *session;
	/** MRCP connection */
	mrcp_connection_t *connection;

	/** waiting state */
	apt_bool_t         waiting;
	/** Media termination */
	mpf_termination_t *termination;
};

typedef struct mrcp_termination_slot_t mrcp_termination_slot_t;
struct mrcp_termination_slot_t {
	/** waiting state */
	apt_bool_t         waiting;
	/** RTP termination */
	mpf_termination_t *termination;
};


typedef enum {
	MRCP_CLIENT_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_CLIENT_CONNECTION_TASK_MSG,
	MRCP_CLIENT_MEDIA_TASK_MSG,
	MRCP_CLIENT_APPLICATION_TASK_MSG
} mrcp_client_task_msg_type_e;

/* Signaling agent interface */
typedef enum {
	SIG_AGENT_TASK_MSG_ANSWER,
	SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE,
	SIG_AGENT_TASK_MSG_TERMINATE_EVENT
} sig_agent_task_msg_type_e ;

typedef struct sig_agent_message_t sig_agent_message_t;
struct sig_agent_message_t {
	mrcp_session_t            *session;
	mrcp_session_descriptor_t *descriptor;
};

static apt_bool_t mrcp_client_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_session_terminate_response(mrcp_session_t *session);
static apt_bool_t mrcp_client_session_terminate_event(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_client_session_answer,
	mrcp_client_session_terminate_response
};

static const mrcp_session_event_vtable_t session_event_vtable = {
	mrcp_client_session_terminate_event
};

/* Connection agent interface */
typedef enum {
	CONNECTION_AGENT_TASK_MSG_MODIFY,
	CONNECTION_AGENT_TASK_MSG_REMOVE,
	CONNECTION_AGENT_TASK_MSG_TERMINATE
} connection_agent_task_msg_type_e ;

typedef struct connection_agent_message_t connection_agent_message_t;
struct connection_agent_message_t {
	mrcp_connection_agent_t   *agent;
	mrcp_channel_t            *channel;
	mrcp_connection_t         *connection;
	mrcp_control_descriptor_t *descriptor;
};

static apt_bool_t mrcp_client_channel_on_modify(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor);
static apt_bool_t mrcp_client_channel_on_remove(
								mrcp_connection_agent_t *agent,
								void *handle);

static const mrcp_connection_event_vtable_t connection_method_vtable = {
	mrcp_client_channel_on_modify,
	mrcp_client_channel_on_remove
};

/* Media interface */
static apt_bool_t mpf_request_send(mrcp_client_t *client, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor);

/* Task interface */
static void mrcp_client_on_start_complete(apt_task_t *task);
static void mrcp_client_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t mrcp_client_application_msg_dispatch(mrcp_client_t *client, mrcp_client_session_t *client_session, const mrcp_app_message_t *app_message);


/** Create MRCP client instance */
MRCP_DECLARE(mrcp_client_t*) mrcp_client_create()
{
	mrcp_client_t *client;
	apr_pool_t *pool;
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}

	apt_log(APT_PRIO_NOTICE,"Create MRCP Client");
	client = apr_palloc(pool,sizeof(mrcp_client_t));
	client->pool = pool;
	client->resource_factory = NULL;
	client->media_engine = NULL;
	client->rtp_termination_factory = NULL;
	client->signaling_agent = NULL;
	client->connection_agent = NULL;
	client->connection_msg_pool = NULL;
	client->application_msg_pool = NULL;
	client->session_table = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = mrcp_client_msg_process;
	vtable.on_start_complete = mrcp_client_on_start_complete;
	vtable.on_terminate_complete = mrcp_client_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);
	client->application_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mrcp_app_message_t*),client->pool);

	client->task = apt_consumer_task_create(client, &vtable, msg_pool, pool);
	if(!client->task) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Client Task");
		return NULL;
	}
	
	return client;
}

/** Start message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_client_start(mrcp_client_t *client)
{
	apt_task_t *task;
	if(!client->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	apt_log(APT_PRIO_INFO,"Start Client Task");
	client->session_table = apr_hash_make(client->pool);
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Start Client Task");
		return FALSE;
	}
	return TRUE;
}

/** Shutdown message processing loop */
MRCP_DECLARE(apt_bool_t) mrcp_client_shutdown(mrcp_client_t *client)
{
	apt_task_t *task;
	if(!client->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	apt_log(APT_PRIO_INFO,"Shutdown Client Task");
	if(apt_task_terminate(task,TRUE) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Shutdown Client Task");
		return FALSE;
	}
	client->session_table = NULL;
	return TRUE;
}

/** Destroy MRCP client */
MRCP_DECLARE(apt_bool_t) mrcp_client_destroy(mrcp_client_t *client)
{
	apt_task_t *task;
	if(!client->task) {
		return FALSE;
	}
	task = apt_consumer_task_base_get(client->task);
	apt_log(APT_PRIO_INFO,"Destroy Client Task");
	apt_task_destroy(task);

	apr_pool_destroy(client->pool);
	return TRUE;
}


/** Register MRCP resource factory */
MRCP_DECLARE(apt_bool_t) mrcp_client_resource_factory_register(mrcp_client_t *client, mrcp_resource_factory_t *resource_factory)
{
	if(!resource_factory) {
		return FALSE;
	}
	client->resource_factory = resource_factory;
	return TRUE;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_client_media_engine_register(mrcp_client_t *client, mpf_engine_t *media_engine)
{
	if(!media_engine) {
		return FALSE;
	}
	client->media_engine = media_engine;
	mpf_engine_task_msg_type_set(media_engine,MRCP_CLIENT_MEDIA_TASK_MSG);
	if(client->task) {
		apt_task_t *media_task = mpf_task_get(media_engine);
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,media_task);
	}
	return TRUE;
}

/** Register RTP termination factory */
MRCP_DECLARE(apt_bool_t) mrcp_client_rtp_termination_factory_register(mrcp_client_t *client, mpf_termination_factory_t *rtp_termination_factory)
{
	if(!rtp_termination_factory) {
		return FALSE;
	}
	client->rtp_termination_factory = rtp_termination_factory;
	return TRUE;
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_client_signaling_agent_register(mrcp_client_t *client, mrcp_sig_agent_t *signaling_agent)
{
	if(!signaling_agent) {
		return FALSE;
	}
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(sig_agent_message_t),client->pool);
	client->signaling_agent = signaling_agent;
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_client_connection_agent_register(mrcp_client_t *client, mrcp_connection_agent_t *connection_agent)
{
	if(!connection_agent) {
		return FALSE;
	}
	mrcp_client_connection_agent_handler_set(connection_agent,client,&connection_method_vtable);
	client->connection_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_message_t),client->pool);
	client->connection_agent = connection_agent;
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_t *connection_task = mrcp_client_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
	return TRUE;
}

/** Register MRCP application */
MRCP_DECLARE(apt_bool_t) mrcp_client_application_register(mrcp_client_t *client, mrcp_application_t *application)
{
	if(!application) {
		return FALSE;
	}
	application->client = client;
	return TRUE;
}

/** Get memory pool */
MRCP_DECLARE(apr_pool_t*) mrcp_client_memory_pool_get(mrcp_client_t *client)
{
	return client->pool;
}





/** Create application instance */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(void *obj, mrcp_version_e version, const mrcp_app_message_handler_f handler, apr_pool_t *pool)
{
	mrcp_application_t *application = apr_palloc(pool,sizeof(mrcp_application_t));
	application->obj = obj;
	application->version = version;
	application->handler = handler;
	application->client = NULL;
	return application;
}

/** Destroy application instance */
MRCP_DECLARE(apt_bool_t) mrcp_application_destroy(mrcp_application_t *application)
{
	return TRUE;
}


/** Create client session */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, void *obj)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*) mrcp_session_create(sizeof(mrcp_client_session_t)-sizeof(mrcp_session_t));
	session->base.response_vtable = &session_response_vtable;
	session->base.event_vtable = &session_event_vtable;

	session->application = application;
	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mrcp_termination_slot_t));
	session->channels = apr_array_make(session->base.pool,2,sizeof(mrcp_channel_t*));
	session->offer = NULL;
	session->answer = NULL;
	session->active_request = NULL;
	session->request_queue = apt_list_create(session->base.pool);
	session->offer_flag_count = 0;
	session->answer_flag_count = 0;
	session->terminate_flag_count = 0;
	return &session->base;
}

/** Get external object associated with the application */
APT_DECLARE(void*) mrcp_application_object_get(mrcp_application_t *application)
{
	return application->obj;
}

static apt_bool_t mrcp_application_task_msg_signal(mrcp_app_command_e command_id, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message, mpf_rtp_termination_descriptor_t *descriptor)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	mrcp_application_t *application = client_session->application;
	apt_task_t *task = apt_consumer_task_base_get(application->client->task);
	apt_task_msg_t *task_msg = apt_task_msg_acquire(application->client->application_msg_pool);
	if(task_msg) {
		mrcp_app_message_t** slot = ((mrcp_app_message_t**)task_msg->data);
		mrcp_app_message_t *app_message;
		task_msg->type = MRCP_CLIENT_APPLICATION_TASK_MSG;

		app_message = apr_palloc(session->pool,sizeof(mrcp_app_message_t));
		app_message->message_type = MRCP_APP_MESSAGE_TYPE_REQUEST;
		app_message->command_id = command_id;
		app_message->application = client_session->application;
		app_message->session = session;
		app_message->channel = channel;
		app_message->mrcp_message = message;
		app_message->descriptor = descriptor;
		*slot = app_message;
	}
	return apt_task_msg_signal(task,task_msg);
}

/** Send session update request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_update(mrcp_session_t *session)
{
	apt_log(APT_PRIO_DEBUG,"Signal Session Update");
	return mrcp_application_task_msg_signal(MRCP_APP_COMMAND_SESSION_UPDATE,session,NULL,NULL,NULL);
}

/** Send session termination request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_terminate(mrcp_session_t *session)
{
	apt_log(APT_PRIO_DEBUG,"Signal Session Terminate");
	return mrcp_application_task_msg_signal(MRCP_APP_COMMAND_SESSION_TERMINATE,session,NULL,NULL,NULL);
}

/** Destroy client session (session must be terminated prior to destroy) */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_destroy(mrcp_session_t *session)
{
	mrcp_session_destroy(session);
	return TRUE;
}


/** Create control channel */
MRCP_DECLARE(mrcp_channel_t*) mrcp_application_channel_create(mrcp_session_t *session, mrcp_resource_id resource_id, mpf_termination_t *termination, void *obj)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->session = session;
	channel->resource_id = resource_id;
	channel->connection = NULL;
	channel->termination = NULL;
	channel->resource = NULL;
	channel->waiting = FALSE;
	return channel;
}

/** Send channel add request */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_add(mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor)
{
	apt_log(APT_PRIO_DEBUG,"Signal Channel Add");
	return mrcp_application_task_msg_signal(MRCP_APP_COMMAND_CHANNEL_ADD,session,channel,NULL,descriptor);
}

/** Remove channel */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_remove(mrcp_session_t *session, mrcp_channel_t *channel)
{
	apt_log(APT_PRIO_DEBUG,"Signal Channel Remove");
	return mrcp_application_task_msg_signal(MRCP_APP_COMMAND_CHANNEL_REMOVE,session,channel,NULL,NULL);
}

/** Send MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_send(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	apt_log(APT_PRIO_DEBUG,"Signal Message Send");
	return mrcp_application_task_msg_signal(MRCP_APP_COMMAND_MESSAGE,session,channel,message,NULL);
}

/** Destroy channel */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_destroy(mrcp_channel_t *channel)
{
	return TRUE;
}





static void mrcp_client_on_start_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Client Task Start");
}

static void mrcp_client_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Client Task Terminate");
}


static apt_bool_t mrcp_client_channel_find(mrcp_client_session_t *session, mrcp_channel_t *channel, int *index)
{
	int i;
	for(i=0; i<session->channels->nelts; i++) {
		mrcp_channel_t *existing_channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(existing_channel == channel) {
			if(index) {
				*index = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}

static mrcp_termination_slot_t* mrcp_client_rtp_termination_find(mrcp_client_session_t *session, mpf_termination_t *termination)
{
	int i;
	mrcp_termination_slot_t *slot;
	for(i=0; i<session->terminations->nelts; i++) {
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(slot && slot->termination == termination) {
			return slot;
		}
	}
	return NULL;
}

static apt_bool_t mrcp_client_application_respond(mrcp_client_t *client, mrcp_client_session_t *session)
{
	mrcp_app_message_t *response;
	if(!session->active_request) {
		return FALSE;
	}
	response = apr_palloc(session->base.pool,sizeof(mrcp_app_message_t));
	*response = *session->active_request;
	response->message_type = MRCP_APP_MESSAGE_TYPE_RESPONSE;
	session->application->handler(response);

	session->active_request = apt_list_pop_front(session->request_queue);
	if(session->active_request) {
		mrcp_client_application_msg_dispatch(client,session,session->active_request);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_channel_modify(mrcp_client_t *client, mrcp_client_session_t *session, mrcp_channel_t *channel, apt_bool_t enable)
{
	int index;
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Modify Control Channel [%d]",enable);
	if(mrcp_client_channel_find(session,channel,&index) == TRUE) {
		mrcp_control_descriptor_t *control_media = mrcp_session_control_media_get(session->offer,(apr_size_t)index);
		if(control_media) {
			control_media->port = (enable == TRUE) ? 9 : 0;
		}
	}

	return mrcp_session_offer(&session->base,session->offer);
}

static apt_bool_t mrcp_client_channel_add(mrcp_client_t *client, mrcp_client_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *rtp_descriptor)
{
	mrcp_channel_t **channel_slot;
	mrcp_control_descriptor_t *control_media;
	mrcp_termination_slot_t *termination_slot;
	mpf_termination_t *termination;
	const apt_str_t *resource_name;
	apr_pool_t *pool = session->base.pool;
	if(mrcp_client_channel_find(session,channel,NULL) == TRUE) {
		/* update */
		return mrcp_client_channel_modify(client,session,channel,TRUE);
	}

	if(!session->offer) {
		session->base.signaling_agent = client->signaling_agent;
		client->signaling_agent->create_client_session(&session->base);

		session->offer = mrcp_session_descriptor_create(pool);
		session->context = mpf_context_create(session,5,pool);
	}
	if(!channel->resource) {
		channel->resource = mrcp_resource_get(client->resource_factory,channel->resource_id);
		if(!channel->resource) {
			return FALSE;
		}
	}

	/* add to channel array */
	apt_log(APT_PRIO_DEBUG,"Add Control Channel");
	channel_slot = apr_array_push(session->channels);
	*channel_slot = channel;
	
	/* create rtp termination */
	termination = mpf_termination_create(client->rtp_termination_factory,session,session->base.pool);
	/* add to channel array */
	apt_log(APT_PRIO_DEBUG,"Add RTP Termination");
	termination_slot = apr_array_push(session->terminations);
	termination_slot->waiting = FALSE;
	termination_slot->termination = termination;
	/* send add termination request (add to media context) */
	if(!rtp_descriptor) {
		rtp_descriptor = apr_palloc(pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
	}
	if(mpf_request_send(client,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor) == TRUE) {
		termination_slot->waiting = TRUE;
		session->offer_flag_count++;
	}

	control_media = apr_palloc(pool,sizeof(mrcp_control_descriptor_t));
	mrcp_control_descriptor_init(control_media);
	control_media->id = mrcp_session_control_media_add(session->offer,control_media);
	control_media->cmid = session->offer->control_media_arr->nelts;

	control_media->proto = MRCP_PROTO_TCP;
	control_media->port = 9;
	control_media->setup_type = MRCP_SETUP_TYPE_ACTIVE;
	control_media->connection_type = MRCP_CONNECTION_TYPE_EXISTING;
	resource_name = mrcp_resource_name_get(client->resource_factory,channel->resource_id,session->application->version);
	if(resource_name) {
		control_media->resource_name = *resource_name;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_update(mrcp_client_t *client, mrcp_client_session_t *session)
{
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Update Session");
	return mrcp_session_offer(&session->base,session->offer);
}

static apt_bool_t mrcp_client_session_terminate(mrcp_client_t *client, mrcp_client_session_t *session)
{
	mrcp_channel_t *channel;
	mrcp_termination_slot_t *slot;
	int i;
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Terminate Session");
	/* remove existing control channels */
	for(i=0; i<session->channels->nelts; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* remove channel */
		apt_log(APT_PRIO_DEBUG,"Remove Control Channel");
		if(mrcp_client_connection_remove(client->connection_agent,channel,channel->connection,channel->pool) == TRUE) {
			channel->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}

	/* subtract existing terminations */
	for(i=0; i<session->terminations->nelts; i++) {
		/* get existing termination */
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* send subtract termination request */
		apt_log(APT_PRIO_DEBUG,"Subtract Termination");
		if(mpf_request_send(client,MPF_COMMAND_SUBTRACT,session->context,slot->termination,NULL) == TRUE) {
			slot->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}

	session->terminate_flag_count++;
	mrcp_session_terminate_request(&session->base);
	return TRUE;
}


static apt_bool_t mrcp_client_on_channel_modify(mrcp_client_t *client, const connection_agent_message_t *message)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)message->channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Modify");
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send application response */
			mrcp_client_application_respond(client,session);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_channel_remove(mrcp_client_t *client, const connection_agent_message_t *message)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)message->channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Remove");
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			/* send application response */
			mrcp_client_application_respond(client,session);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_modify(mrcp_client_t *client, mrcp_client_session_t *session, const mpf_message_t *mpf_message)
{
	if(session && session->offer) {
		mrcp_termination_slot_t *termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
		if(termination_slot && termination_slot->waiting == TRUE) {
			mpf_rtp_termination_descriptor_t *rtp_descriptor = mpf_message->descriptor;
			if(rtp_descriptor->audio.local) {
				session->offer->ip = rtp_descriptor->audio.local->base.ip;
				rtp_descriptor->audio.local->base.id = mrcp_session_audio_media_add(session->offer,rtp_descriptor->audio.local);
				rtp_descriptor->audio.local->mid = session->offer->audio_media_arr->nelts;
			}
			termination_slot->waiting = FALSE;
			if(session->offer_flag_count) {
				session->offer_flag_count--;
				if(!session->offer_flag_count) {
					mrcp_session_offer(&session->base,session->offer);
				}
			}
			if(session->answer_flag_count) {
				session->answer_flag_count--;
				if(!session->answer_flag_count) {
					/* send application response */
					mrcp_client_application_respond(client,session);
				}
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_subtract(mrcp_client_t *client, mrcp_client_session_t *session, const mpf_message_t *mpf_message)
{
	if(session && session->offer) {
		mrcp_termination_slot_t *termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
		if(termination_slot && termination_slot->waiting == TRUE) {
			if(session->terminate_flag_count) {
				session->terminate_flag_count--;
				if(!session->terminate_flag_count) {
					/* send application response */
					mrcp_client_application_respond(client,session);
				}
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_control_media_answer_process(mrcp_client_t *client, mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_channel_t *channel;
	mrcp_control_descriptor_t *control_descriptor;
	int i;
	int count = session->channels->nelts;
	if(count != descriptor->control_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of control channels [%d] != Number of control media in answer [%d]",
			count,descriptor->control_media_arr->nelts);
		count = descriptor->control_media_arr->nelts;
	}

	if(!session->base.id.length) {
		/* initial answer received, store session id and add to session's table */
		control_descriptor = mrcp_session_control_media_get(descriptor,0);
		if(control_descriptor) {
			session->base.id = control_descriptor->session_id;
			apt_log(APT_PRIO_NOTICE,"Add Session <%s>",session->base.id.buf);
			apr_hash_set(client->session_table,session->base.id.buf,session->base.id.length,session);
		}
	}

	/* update existing control channels */
	for(i=0; i<count; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		/* modify channel */
		apt_log(APT_PRIO_DEBUG,"Modify Control Channel");
		if(mrcp_client_connection_modify(client->connection_agent,channel,channel->connection,control_descriptor,channel->pool) == TRUE) {
			channel->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_av_media_answer_process(mrcp_client_t *client, mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_termination_slot_t *slot;
	int i;
	int count = session->terminations->nelts;
	if(count != descriptor->audio_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of terminations [%d] != Number of audio media in answer [%d]\n",
			count,descriptor->audio_media_arr->nelts);
		count = descriptor->audio_media_arr->nelts;
	}
	
	/* update existing terminations */
	for(i=0; i<count; i++) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		/* get existing termination */
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send modify termination request */
		apt_log(APT_PRIO_DEBUG,"Modify Termination");
		if(mpf_request_send(client,MPF_COMMAND_MODIFY,session->context,slot->termination,rtp_descriptor) == TRUE) {
			slot->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_answer_process(mrcp_client_t *client, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	apt_log(APT_PRIO_INFO,"Process Session Answer <%s> [control:%d audio:%d video:%d]",
		session->id.buf ? session->id.buf : "new",
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);

	mrcp_client_control_media_answer_process(client,client_session,descriptor);
	mrcp_client_av_media_answer_process(client,client_session,descriptor);

	/* store received answer */
	client_session->answer = descriptor;

	if(!client_session->answer_flag_count) {
		mrcp_client_application_respond(client,client_session);
	}

	return TRUE;
}

static apt_bool_t mrcp_client_session_terminate_response_process(mrcp_client_t *client, mrcp_session_t *session)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	apt_log(APT_PRIO_INFO,"Process Session Terminate <%s>",session->id.buf);

	if(client_session->terminate_flag_count) {
		client_session->terminate_flag_count--;
	}

	if(!client_session->terminate_flag_count) {
		mrcp_client_application_respond(client,client_session);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_terminate_event_process(mrcp_client_t *client, mrcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t mrcp_client_application_msg_dispatch(mrcp_client_t *client, mrcp_client_session_t *client_session, const mrcp_app_message_t *app_message)
{
	switch(app_message->command_id) {
		case MRCP_APP_COMMAND_SESSION_UPDATE:
			mrcp_client_session_update(client,client_session);
			break;
		case MRCP_APP_COMMAND_SESSION_TERMINATE:
			mrcp_client_session_terminate(client,client_session);
			break;
		case MRCP_APP_COMMAND_CHANNEL_ADD:
			mrcp_client_channel_add(client,client_session,app_message->channel,app_message->descriptor);
			break;
		case MRCP_APP_COMMAND_CHANNEL_REMOVE:
			mrcp_client_channel_modify(client,client_session,app_message->channel,FALSE);
			break;
		case MRCP_APP_COMMAND_MESSAGE:
			break;
		default:
			break;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_client_t *client = apt_consumer_task_object_get(consumer_task);
	if(!client) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Process Client Task Message [%d]", msg->type);
	switch(msg->type) {
		case MRCP_CLIENT_SIGNALING_TASK_MSG:
		{
			const sig_agent_message_t *sig_message = (const sig_agent_message_t*)msg->data;
			switch(msg->sub_type) {
				case SIG_AGENT_TASK_MSG_ANSWER:
					mrcp_client_session_answer_process(client,sig_message->session,sig_message->descriptor);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE:
					mrcp_client_session_terminate_response_process(client,sig_message->session);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE_EVENT:
					mrcp_client_session_terminate_event_process(client,sig_message->session);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_CLIENT_CONNECTION_TASK_MSG:
		{
			const connection_agent_message_t *connection_message = (const connection_agent_message_t*)msg->data;
			switch(msg->sub_type) {
				case CONNECTION_AGENT_TASK_MSG_MODIFY:
				{
					mrcp_client_on_channel_modify(client,connection_message);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_REMOVE:
				{
					mrcp_client_on_channel_remove(client,connection_message);
					break;
				}
				default:
					break;
			}
			break;
		}
		case MRCP_CLIENT_MEDIA_TASK_MSG:
		{
			mrcp_client_session_t *session = NULL;
			const mpf_message_t *mpf_message = (const mpf_message_t*) msg->data;
			if(mpf_message->termination) {
				session = mpf_termination_object_get(mpf_message->termination);
			}
			if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
				if(mpf_message->command_id == MPF_COMMAND_ADD) {
					apt_log(APT_PRIO_DEBUG,"On Termination Add");
					mrcp_client_on_termination_modify(client,session,mpf_message);
				}
				else if(mpf_message->command_id == MPF_COMMAND_MODIFY) {
					apt_log(APT_PRIO_DEBUG,"On Termination Modify");
					mrcp_client_on_termination_modify(client,session,mpf_message);
				}
				else if(mpf_message->command_id == MPF_COMMAND_SUBTRACT) {
					apt_log(APT_PRIO_DEBUG,"On Termination Subtract");
					mrcp_client_on_termination_subtract(client,session,mpf_message);
				}
			}
			else if(mpf_message->message_type == MPF_MESSAGE_TYPE_EVENT) {
				apt_log(APT_PRIO_DEBUG,"Process MPF Event");
			}
			break;
		}
		case MRCP_CLIENT_APPLICATION_TASK_MSG:
		{
			mrcp_client_session_t *client_session;
			mrcp_app_message_t **slot = (mrcp_app_message_t**) msg->data;
			mrcp_app_message_t *app_message = *slot;
			if(app_message->message_type != MRCP_APP_MESSAGE_TYPE_REQUEST) {
				break;
			}
			client_session = (mrcp_client_session_t*)app_message->session;
			if(client_session->active_request) {
				apt_list_push_back(client_session->request_queue,app_message);
			}
			else {
				client_session->active_request = app_message;
				mrcp_client_application_msg_dispatch(client,client_session,app_message);
			}
			break;
		}
		default:
			break;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	sig_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_CLIENT_SIGNALING_TASK_MSG;
	task_msg->sub_type = SIG_AGENT_TASK_MSG_ANSWER;
	message = (sig_agent_message_t*) task_msg->data;
	message->session = session;
	message->descriptor = descriptor;

	apt_log(APT_PRIO_DEBUG,"Signal Session Answer");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_client_session_terminate_response(mrcp_session_t *session)
{
	sig_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_CLIENT_SIGNALING_TASK_MSG;
	task_msg->sub_type = SIG_AGENT_TASK_MSG_TERMINATE_RESPONSE;
	message = (sig_agent_message_t*) task_msg->data;
	message->session = session;
	message->descriptor = NULL;

	apt_log(APT_PRIO_DEBUG,"Signal Session Terminate Response");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_client_session_terminate_event(mrcp_session_t *session)
{
	sig_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_CLIENT_SIGNALING_TASK_MSG;
	task_msg->sub_type = SIG_AGENT_TASK_MSG_TERMINATE_EVENT;
	message = (sig_agent_message_t*) task_msg->data;
	message->session = session;
	message->descriptor = NULL;

	apt_log(APT_PRIO_DEBUG,"Signal Session Terminate Event");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_client_channel_on_modify(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor)
{
	mrcp_client_t *client = mrcp_client_connection_agent_object_get(agent);
	apt_task_t *task = apt_consumer_task_base_get(client->task);
	connection_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(client->connection_msg_pool);
	task_msg->type = MRCP_CLIENT_CONNECTION_TASK_MSG;
	task_msg->sub_type = CONNECTION_AGENT_TASK_MSG_MODIFY;
	message = (connection_agent_message_t*) task_msg->data;
	message->agent = agent;
	message->channel = handle;
	message->connection = connection;
	message->descriptor = descriptor;

	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_client_channel_on_remove(
								mrcp_connection_agent_t *agent,
								void *handle)
{
	mrcp_client_t *client = mrcp_client_connection_agent_object_get(agent);
	apt_task_t *task = apt_consumer_task_base_get(client->task);
	connection_agent_message_t *message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(client->connection_msg_pool);
	task_msg->type = MRCP_CLIENT_CONNECTION_TASK_MSG;
	task_msg->sub_type = CONNECTION_AGENT_TASK_MSG_REMOVE;
	message = (connection_agent_message_t*) task_msg->data;
	message->agent = agent;
	message->channel = handle;
	message->connection = NULL;
	message->descriptor = NULL;

	return apt_task_msg_signal(task,task_msg);
}

/* Media interface */
static apt_bool_t mpf_request_send(mrcp_client_t *client, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor)
{
	apt_task_t *media_task = mpf_task_get(client->media_engine);
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
