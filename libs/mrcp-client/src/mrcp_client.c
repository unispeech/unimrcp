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
#include "mrcp_application.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_client_connection.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "apt_consumer_task.h"
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
	void                            *obj;
	const mrcp_application_vtable_t *vtable;
	mrcp_client_t                   *client;
};

typedef struct mrcp_client_session_t mrcp_client_session_t;
struct mrcp_client_session_t {
	/** Session base */
	mrcp_session_t base;
	/** Application session belongs to */
	mrcp_application_t *application;

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
	MRCP_CLIENT_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_CLIENT_CONNECTION_TASK_MSG,
	MRCP_CLIENT_MEDIA_TASK_MSG,
	MRCP_CLIENT_APPLICATION_TASK_MSG
} mrcp_client_task_msg_type_e;

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

static apt_bool_t mrcp_client_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_session_terminate(mrcp_session_t *session);

static const mrcp_session_method_vtable_t session_method_vtable = {
	NULL, /* offer */
	mrcp_client_session_answer,
	mrcp_client_session_terminate
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

/* Application interface */
typedef enum {
	APPLICATION_TASK_MSG_SESSION_UPDATE,
	APPLICATION_TASK_MSG_SESSION_TERMINATE,
	APPLICATION_TASK_MSG_CHANNEL_MODIFY,
	APPLICATION_TASK_MSG_CHANNEL_REMOVE,
	APPLICATION_TASK_MSG_MESSAGE_SEND,
} application_task_msg_type_e ;

typedef struct application_message_t application_message_t;
struct application_message_t {
	mrcp_session_t             *session;
	mrcp_channel_t             *channel;
	mrcp_message_t             *message;
	mpf_rtp_media_descriptor_t *descriptor;
};


/* Task interface */
static void mrcp_client_on_start_complete(apt_task_t *task);
static void mrcp_client_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg);

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
	client->application_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(application_message_t),client->pool);

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
/*	mrcp_client_connection_agent_handler_set(connection_agent,client,&connection_method_vtable);
	client->connection_msg_pool = apt_task_msg_pool_create_dynamic(sizeof(connection_agent_message_t),client->pool);
	client->connection_agent = connection_agent;
	if(client->task) {
		apt_task_t *task = apt_consumer_task_base_get(client->task);
		apt_task_t *connection_task = mrcp_client_connection_agent_task_get(connection_agent);
		apt_task_add(task,connection_task);
	}
*/	return TRUE;
}

/** Get memory pool */
MRCP_DECLARE(apr_pool_t*) mrcp_client_memory_pool_get(mrcp_client_t *client)
{
	return client->pool;
}





/** Create application instance */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(void *obj, const mrcp_application_vtable_t *vtable, apr_pool_t *pool)
{
	mrcp_application_t *application = apr_palloc(pool,sizeof(mrcp_application_t));
	application->obj = obj;
	application->vtable = vtable;
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
	session->base.method_vtable = &session_method_vtable;

	session->application = application;
	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mpf_termination_t*));
	session->channels = apr_array_make(session->base.pool,2,sizeof(mrcp_channel_t*));
	session->offer = NULL;
	session->answer = NULL;
	return &session->base;
}

static apt_bool_t mrcp_application_task_msg_signal(int type, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message, mpf_rtp_media_descriptor_t *descriptor)
{
	mrcp_client_session_t *client_session = (mrcp_client_session_t*)session;
	mrcp_application_t *application = client_session->application;
	apt_task_t *task = apt_consumer_task_base_get(application->client->task);
	application_message_t *app_message;
	apt_task_msg_t *task_msg = apt_task_msg_acquire(session->signaling_agent->msg_pool);
	task_msg->type = MRCP_CLIENT_APPLICATION_TASK_MSG;
	task_msg->sub_type = type;
	app_message = (application_message_t*) task_msg->data;
	app_message->session = session;
	app_message->channel = channel;
	app_message->message = message;
	app_message->descriptor = descriptor;
	return apt_task_msg_signal(task,task_msg);
}

/** Send session update request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_update(mrcp_session_t *session)
{
	apt_log(APT_PRIO_DEBUG,"Signal Session Update");
	return mrcp_application_task_msg_signal(APPLICATION_TASK_MSG_SESSION_UPDATE,session,NULL,NULL,NULL);
}

/** Send session termination request */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_terminate(mrcp_session_t *session)
{
	apt_log(APT_PRIO_DEBUG,"Signal Session Terminate");
	return mrcp_application_task_msg_signal(APPLICATION_TASK_MSG_SESSION_TERMINATE,session,NULL,NULL,NULL);
}

/** Destroy client session (session must be terminated prior to destroy) */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_destroy(mrcp_session_t *session)
{
	mrcp_session_destroy(session);
	return TRUE;
}


/** Create control channel */
MRCP_DECLARE(mrcp_channel_t*) mrcp_application_channel_create(mrcp_session_t *session, mpf_termination_t *termination, void *obj)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->session = session;
	channel->connection = NULL;
	channel->termination = NULL;
	channel->resource = NULL;
	return channel;
}

/** Send channel add/modify request */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_modify(mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_media_descriptor_t *descriptor)
{
	apt_log(APT_PRIO_DEBUG,"Signal Channel Modify");
	return mrcp_application_task_msg_signal(APPLICATION_TASK_MSG_CHANNEL_MODIFY,session,channel,NULL,descriptor);
}

/** Send MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_send(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	apt_log(APT_PRIO_DEBUG,"Signal Message Send");
	return mrcp_application_task_msg_signal(APPLICATION_TASK_MSG_MESSAGE_SEND,session,channel,message,NULL);
}

/** Remove channel */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_remove(mrcp_session_t *session, mrcp_channel_t *channel)
{
	apt_log(APT_PRIO_DEBUG,"Signal Channel Remove");
	return mrcp_application_task_msg_signal(APPLICATION_TASK_MSG_CHANNEL_REMOVE,session,channel,NULL,NULL);
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

static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
//	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
//	mrcp_client_t *client = apt_consumer_task_object_get(consumer_task);
	apt_log(APT_PRIO_DEBUG,"Process Client Task Message [%d]", msg->type);
	switch(msg->type) {
		case MRCP_CLIENT_SIGNALING_TASK_MSG:
		{
			break;
		}
		case MRCP_CLIENT_CONNECTION_TASK_MSG:
		{
			break;
		}
		case MRCP_CLIENT_MEDIA_TASK_MSG:
		{
			break;
		}
		case MRCP_CLIENT_APPLICATION_TASK_MSG:
		{
			break;
		}
		default:
			break;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return TRUE;
}

static apt_bool_t mrcp_client_session_terminate(mrcp_session_t *session)
{
	return TRUE;
}
