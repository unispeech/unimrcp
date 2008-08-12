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
#include "mrcp_server.h"
#include "mrcp_server_session.h"
#include "mrcp_message.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_server_connection.h"
#include "mpf_engine.h"
#include "apt_consumer_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define MRCP_SESSION_ID_HEX_STRING_LENGTH 16

/** MRCP server */
struct mrcp_server_t {
	/** Main message processing task */
	apt_consumer_task_t       *task;

	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** MRCP resource engine list */
	apt_obj_list_t            *resource_engines;
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
	/** Resource engine task message pool */
	apt_task_msg_pool_t       *resource_engine_msg_pool;
	
	/** MRCP sessions table */
	apr_hash_t                *session_table;

	/** Memory pool */
	apr_pool_t                *pool;
};


typedef enum {
	MRCP_SERVER_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_SERVER_CONNECTION_TASK_MSG,
	MRCP_SERVER_RESOURCE_ENGINE_TASK_MSG,
	MRCP_SERVER_MEDIA_TASK_MSG
} mrcp_server_task_msg_type_e;


static apt_bool_t mrcp_server_offer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_server_terminate_signal(mrcp_session_t *session);

static const mrcp_session_request_vtable_t session_request_vtable = {
	mrcp_server_offer_signal,
	mrcp_server_terminate_signal
};


/* Connection agent interface */
typedef enum {
	CONNECTION_AGENT_TASK_MSG_MODIFY_CONNECTION,
	CONNECTION_AGENT_TASK_MSG_REMOVE_CONNECTION,
	CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
	CONNECTION_AGENT_TASK_MSG_TERMINATE
} connection_agent_task_msg_type_e;

typedef struct connection_agent_task_msg_data_t connection_agent_task_msg_data_t;
struct connection_agent_task_msg_data_t {
	mrcp_channel_t            *channel;
	mrcp_connection_t         *connection;
	mrcp_control_descriptor_t *descriptor;
	mrcp_message_t            *message;
};

static apt_bool_t mrcp_server_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor);
static apt_bool_t mrcp_server_channel_remove_signal(mrcp_control_channel_t *channel);
static apt_bool_t mrcp_server_message_signal(mrcp_connection_agent_t *agent, mrcp_connection_t *connection,	mrcp_message_t *message);

static const mrcp_connection_event_vtable_t connection_method_vtable = {
	mrcp_server_channel_modify_signal,
	mrcp_server_channel_remove_signal,
	mrcp_server_message_signal
};


/* Resource engine interface */
typedef enum {
	RESOURCE_ENGINE_TASK_MSG_OPEN_CHANNEL,
	RESOURCE_ENGINE_TASK_MSG_CLOSE_CHANNEL,
	RESOURCE_ENGINE_TASK_MSG_MESSAGE
} resource_engine_task_msg_type_e;

typedef struct resource_engine_task_msg_data_t resource_engine_task_msg_data_t;
struct resource_engine_task_msg_data_t {
	mrcp_channel_t *channel;
	apt_bool_t      status;
	mrcp_message_t *mrcp_message;
};

static apt_bool_t mrcp_server_channel_open_signal(mrcp_engine_channel_t *channel, apt_bool_t status);
static apt_bool_t mrcp_server_channel_close_signal(mrcp_engine_channel_t *channel);
static apt_bool_t mrcp_server_channel_message_signal(mrcp_engine_channel_t *channel, mrcp_message_t *message);

const mrcp_engine_channel_event_vtable_t engine_channel_vtable = {
	mrcp_server_channel_open_signal,
	mrcp_server_channel_close_signal,
	mrcp_server_channel_message_signal
};

/* Task interface */
static void mrcp_server_on_start_complete(apt_task_t *task);
static void mrcp_server_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static mrcp_session_t* mrcp_server_sig_agent_session_create(mrcp_sig_agent_t *signaling_agent);


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
	server->resource_engines = apt_list_create(pool);
	server->media_engine = NULL;
	server->rtp_termination_factory = NULL;
	server->signaling_agent = NULL;
	server->connection_agent = NULL;
	server->connection_msg_pool = NULL;
	server->resource_engine_msg_pool = NULL;
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
	if(!resource_factory) {
		return FALSE;
	}
	server->resource_factory = resource_factory;
	return TRUE;
}

/** Register MRCP resource engine */
MRCP_DECLARE(apt_bool_t) mrcp_server_resource_engine_register(mrcp_server_t *server, mrcp_resource_engine_t *engine)
{
	if(!engine) {
		return FALSE;
	}
	if(!server->resource_engine_msg_pool) {
		server->resource_engine_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(resource_engine_task_msg_data_t),server->pool);
	}
	apt_list_push_back(server->resource_engines,engine);
	return TRUE;
}

/** Register media engine */
MRCP_DECLARE(apt_bool_t) mrcp_server_media_engine_register(mrcp_server_t *server, mpf_engine_t *media_engine, const char *name)
{
	if(!media_engine || !name) {
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
MRCP_DECLARE(apt_bool_t) mrcp_server_rtp_factory_register(mrcp_server_t *server, mpf_termination_factory_t *rtp_termination_factory, const char *name)
{
	if(!rtp_termination_factory || !name) {
		return FALSE;
	}
	server->rtp_termination_factory = rtp_termination_factory;
	return TRUE;
}

/** Register MRCP signaling agent */
MRCP_DECLARE(apt_bool_t) mrcp_server_signaling_agent_register(mrcp_server_t *server, mrcp_sig_agent_t *signaling_agent, const char *name)
{
	if(!signaling_agent || !name) {
		return FALSE;
	}
	signaling_agent->parent = server;
	signaling_agent->create_server_session = mrcp_server_sig_agent_session_create;
	signaling_agent->msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mrcp_signaling_message_t*),server->pool);
	server->signaling_agent = signaling_agent;
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,signaling_agent->task);
	}
	return TRUE;
}

/** Register MRCP connection agent (MRCPv2 only) */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_register(mrcp_server_t *server, mrcp_connection_agent_t *connection_agent, const char *name)
{
	if(!connection_agent || !name) {
		return FALSE;
	}
	mrcp_server_connection_resource_factory_set(connection_agent,server->resource_factory);
	mrcp_server_connection_agent_handler_set(connection_agent,server,&connection_method_vtable);
	server->connection_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_task_msg_data_t),server->pool);
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

void mrcp_server_session_add(mrcp_server_session_t *session)
{
	if(session->base.id.buf) {
		apt_log(APT_PRIO_NOTICE,"Add Session <%s>",session->base.id.buf);
		apr_hash_set(session->server->session_table,session->base.id.buf,session->base.id.length,session);
	}
}

void mrcp_server_session_remove(mrcp_server_session_t *session)
{
	if(session->base.id.buf) {
		apt_log(APT_PRIO_NOTICE,"Remove Session <%s>",session->base.id.buf);
		apr_hash_set(session->server->session_table,session->base.id.buf,session->base.id.length,NULL);
	}
}

static APR_INLINE mrcp_server_session_t* mrcp_server_session_find(mrcp_server_t *server, const apt_str_t *session_id)
{
	return apr_hash_get(server->session_table,session_id->buf,session_id->length);
}

static void mrcp_server_on_start_complete(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);
	mrcp_resource_engine_t *resource_engine;
	apt_list_elem_t *elem;
	apt_log(APT_PRIO_INFO,"Open Resource Engines");
	elem = apt_list_first_elem_get(server->resource_engines);
	/* walk through the list of engines */
	while(elem) {
		resource_engine = apt_list_elem_object_get(elem);
		if(resource_engine) {
			mrcp_resource_engine_open(resource_engine);
		}
		elem = apt_list_next_elem_get(server->resource_engines,elem);
	}
	apt_log(APT_PRIO_INFO,"On Server Task Start");
}

static void mrcp_server_on_terminate_complete(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);
	mrcp_resource_engine_t *resource_engine;
	apt_list_elem_t *elem;
	apt_log(APT_PRIO_INFO,"Close Resource Engines");
	elem = apt_list_first_elem_get(server->resource_engines);
	/* walk through the list of engines */
	while(elem) {
		resource_engine = apt_list_elem_object_get(elem);
		if(resource_engine) {
			mrcp_resource_engine_close(resource_engine);
		}
		elem = apt_list_next_elem_get(server->resource_engines,elem);
	}
	apt_log(APT_PRIO_INFO,"On Server Task Terminate");
}

static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	switch(msg->type) {
		case MRCP_SERVER_SIGNALING_TASK_MSG:
		{
			mrcp_signaling_message_t **signaling_message = (mrcp_signaling_message_t**) msg->data;
			apt_log(APT_PRIO_DEBUG,"Receive Signaling Task Message [%d]", (*signaling_message)->type);
			mrcp_server_signaling_message_process(*signaling_message);
			break;
		}
		case MRCP_SERVER_CONNECTION_TASK_MSG:
		{
			const connection_agent_task_msg_data_t *connection_message = (const connection_agent_task_msg_data_t*)msg->data;
			apt_log(APT_PRIO_DEBUG,"Receive Connection Task Message [%d]", msg->sub_type);
			switch(msg->sub_type) {
				case CONNECTION_AGENT_TASK_MSG_MODIFY_CONNECTION:
				{
					mrcp_server_on_channel_modify(connection_message->channel,connection_message->connection,connection_message->descriptor);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_REMOVE_CONNECTION:
				{
					mrcp_server_on_channel_remove(connection_message->channel);
					break;
				}
				case CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE:
				{
					mrcp_signaling_message_t *signaling_message;
					apt_consumer_task_t *consumer_task = apt_task_object_get(task);
					mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);

					mrcp_message_t *message = connection_message->message;
					mrcp_server_session_t *session = mrcp_server_session_find(
												server,
												&message->channel_id.session_id);
					if(!session) {
						apt_log(APT_PRIO_WARNING,"No Such Session <%s>", message->channel_id.session_id.buf);
						break;
					}

					signaling_message = apr_palloc(session->base.pool,sizeof(mrcp_signaling_message_t));
					signaling_message->type = SIGNALING_MESSAGE_CONTROL;
					signaling_message->session = session;
					signaling_message->descriptor = NULL;
					signaling_message->message = message;
					mrcp_server_signaling_message_process(signaling_message);
					break;
				}
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_RESOURCE_ENGINE_TASK_MSG:
		{
			resource_engine_task_msg_data_t *data = (resource_engine_task_msg_data_t*)msg->data;
			apt_log(APT_PRIO_DEBUG,"Receive Resource Engine Task Message [%d]", msg->sub_type);
			switch(msg->sub_type) {
				case RESOURCE_ENGINE_TASK_MSG_OPEN_CHANNEL:
					mrcp_server_on_engine_channel_open(data->channel,data->status);
					break;
				case RESOURCE_ENGINE_TASK_MSG_CLOSE_CHANNEL:
					mrcp_server_on_engine_channel_close(data->channel);
					break;
				case RESOURCE_ENGINE_TASK_MSG_MESSAGE:
					mrcp_server_on_engine_channel_message(data->channel,data->mrcp_message);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_MEDIA_TASK_MSG:
		{
			mpf_message_t *mpf_message = (mpf_message_t*) msg->data;
			apt_log(APT_PRIO_DEBUG,"Receive Media Task Message [%d]", mpf_message->command_id);
			mrcp_server_mpf_message_process(mpf_message);
			break;
		}
		default:
		{
			apt_log(APT_PRIO_WARNING,"Receive Unknown Task Message [%d]", msg->type);
			break;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_signaling_task_msg_signal(mrcp_signaling_message_type_e type, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_message_t *message)
{
	mrcp_signaling_message_t *signaling_message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	mrcp_signaling_message_t **slot = ((mrcp_signaling_message_t**)task_msg->data);
	task_msg->type = MRCP_SERVER_SIGNALING_TASK_MSG;
	task_msg->sub_type = type;
	
	signaling_message = apr_palloc(session->pool,sizeof(mrcp_signaling_message_t));
	signaling_message->type = type;
	signaling_message->session = (mrcp_server_session_t*)session;
	signaling_message->descriptor = descriptor;
	signaling_message->message = message;
	*slot = signaling_message;
	
	apt_log(APT_PRIO_DEBUG,"Signal Signaling Task Message");
	return apt_task_msg_parent_signal(session->signaling_agent->task,task_msg);
}

static apt_bool_t mrcp_server_connection_task_msg_signal(
							connection_agent_task_msg_type_e type,
							mrcp_connection_agent_t         *agent,
							mrcp_control_channel_t          *channel,
							mrcp_connection_t               *connection,
							mrcp_control_descriptor_t       *descriptor,
							mrcp_message_t                  *message)
{
	mrcp_server_t *server = mrcp_server_connection_agent_object_get(agent);
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	connection_agent_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->connection_msg_pool);
	task_msg->type = MRCP_SERVER_CONNECTION_TASK_MSG;
	task_msg->sub_type = type;
	data = (connection_agent_task_msg_data_t*) task_msg->data;
	data->channel = channel ? channel->obj : NULL;
	data->connection = connection;
	data->descriptor = descriptor;
	data->message = message;

	apt_log(APT_PRIO_DEBUG,"Signal Connection Task Message");
	return apt_task_msg_signal(task,task_msg);
}

static apt_bool_t mrcp_server_engine_task_msg_signal(
							resource_engine_task_msg_type_e  type,
							mrcp_engine_channel_t           *engine_channel,
							apt_bool_t                       status,
							mrcp_message_t                  *message)
{
	mrcp_channel_t *channel = engine_channel->event_obj;
	mrcp_session_t *session = mrcp_server_channel_session_get(channel);
	mrcp_server_t *server = session->signaling_agent->parent;
	apt_task_t *task = apt_consumer_task_base_get(server->task);
	resource_engine_task_msg_data_t *data;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(server->resource_engine_msg_pool);
	task_msg->type = MRCP_SERVER_RESOURCE_ENGINE_TASK_MSG;
	task_msg->sub_type = type;
	data = (resource_engine_task_msg_data_t*) task_msg->data;
	data->channel = channel;
	data->status = status;
	data->mrcp_message = message;

	apt_log(APT_PRIO_DEBUG,"Signal Resource Engine Task Message");
	return apt_task_msg_signal(task,task_msg);
}


static mrcp_session_t* mrcp_server_sig_agent_session_create(mrcp_sig_agent_t *signaling_agent)
{
	mrcp_server_t *server = signaling_agent->parent;
	mrcp_server_session_t *session = mrcp_server_session_create();
	session->server = server;
	session->resource_factory = server->resource_factory;
	session->resource_engines = server->resource_engines;
	session->media_engine = server->media_engine;
	session->rtp_termination_factory = server->rtp_termination_factory;
	session->connection_agent = server->connection_agent;

	session->base.signaling_agent = signaling_agent;
	session->base.request_vtable = &session_request_vtable;
	return &session->base;
}

static apt_bool_t mrcp_server_offer_signal(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return mrcp_server_signaling_task_msg_signal(SIGNALING_MESSAGE_OFFER,session,descriptor,NULL);
}

static apt_bool_t mrcp_server_terminate_signal(mrcp_session_t *session)
{
	return mrcp_server_signaling_task_msg_signal(SIGNALING_MESSAGE_TERMINATE,session,NULL,NULL);
}

static apt_bool_t mrcp_server_channel_modify_signal(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_MODIFY_CONNECTION,
								agent,
								channel,
								NULL,
								descriptor,
								NULL);
}

static apt_bool_t mrcp_server_channel_remove_signal(mrcp_control_channel_t *channel)
{
	mrcp_connection_agent_t *agent = channel->agent;
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_REMOVE_CONNECTION,
								agent,
								channel,
								NULL,
								NULL,
								NULL);
}

static apt_bool_t mrcp_server_message_signal(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, mrcp_message_t *message)
{
	return mrcp_server_connection_task_msg_signal(
								CONNECTION_AGENT_TASK_MSG_RECEIVE_MESSAGE,
								agent,
								NULL,
								connection,
								NULL,
								message);
}

static apt_bool_t mrcp_server_channel_open_signal(mrcp_engine_channel_t *channel, apt_bool_t status)
{
	return mrcp_server_engine_task_msg_signal(
								RESOURCE_ENGINE_TASK_MSG_OPEN_CHANNEL,
								channel,
								status,
								NULL);
}

static apt_bool_t mrcp_server_channel_close_signal(mrcp_engine_channel_t *channel)
{
	return mrcp_server_engine_task_msg_signal(
								RESOURCE_ENGINE_TASK_MSG_CLOSE_CHANNEL,
								channel,
								TRUE,
								NULL);
}

static apt_bool_t mrcp_server_channel_message_signal(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_server_engine_task_msg_signal(
								RESOURCE_ENGINE_TASK_MSG_MESSAGE,
								channel,
								TRUE,
								message);
}
