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
	
	/** MRCP sessions table */
	apr_hash_t                *session_table;

	/** Memory pool */
	apr_pool_t                *pool;
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
	MRCP_CLIENT_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_CLIENT_CONNECTION_TASK_MSG,
	MRCP_CLIENT_MEDIA_TASK_MSG
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


/* Task interface */
static void mrcp_client_on_start_complete(apt_task_t *task);
static void mrcp_client_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_client_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static mrcp_session_t* mrcp_client_session_create();

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
	client->session_table = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = mrcp_client_msg_process;
	vtable.on_start_complete = mrcp_client_on_start_complete;
	vtable.on_terminate_complete = mrcp_client_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);

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
	signaling_agent->create_session = mrcp_client_session_create;
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

static void mrcp_client_on_start_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Client Task Start");
}

static void mrcp_client_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Client Task Terminate");
}

static mrcp_session_t* mrcp_client_session_create()
{
	return NULL;
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
		default:
			break;
	}
	return TRUE;
}
