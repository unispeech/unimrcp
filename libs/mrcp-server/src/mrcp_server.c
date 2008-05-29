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
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_session.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define MRCP_SESSION_ID_HEX_STRING_LENGTH 16

/** MRCP server */
struct mrcp_server_t {
	/** Main message processing task */
	apt_consumer_task_t        *task;

	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** Media processing engine */
	apt_task_t                *media_engine;
	/** Signaling agent */
	mrcp_sig_agent_t          *signaling_agent;
	
	/** MRCP sessions table */
	apr_hash_t                *session_table;

	/** Memory pool */
	apr_pool_t                *pool;
};

typedef enum {
	MRCP_SERVER_SIGNALING_TASK_MSG = TASK_MSG_USER,
	MRCP_SERVER_MEDIA_TASK_MSG
} mrcp_server_task_msg_type_e ;


typedef enum {
	SIG_AGENT_TASK_MSG_OFFER,
	SIG_AGENT_TASK_MSG_TERMINATE
}sig_agent_task_msg_type_e ;

typedef struct sig_agent_message_t sig_agent_message_t;
struct sig_agent_message_t {
	mrcp_session_t            *session;
	mrcp_session_descriptor_t *descriptor;
};

static void mrcp_server_on_start_complete(apt_task_t *task);
static void mrcp_server_on_terminate_complete(apt_task_t *task);
static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg);
static mrcp_session_t* mrcp_server_session_create();

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
	server->signaling_agent = NULL;
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
MRCP_DECLARE(apt_bool_t) mrcp_server_media_engine_register(mrcp_server_t *server, apt_task_t *media_engine)
{
	if(!media_engine) {
		return FALSE;
	}
	server->media_engine = media_engine;
	if(server->task) {
		apt_task_t *task = apt_consumer_task_base_get(server->task);
		apt_task_add(task,media_engine);
	}
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


static apt_bool_t mrcp_server_session_offer_process(mrcp_server_t *server, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	apt_log(APT_PRIO_INFO,"Process Session Offer");
	if(!session->id.length) {
		/* Received first offer, generate session id and add to session's table */
		apt_unique_id_generate(&session->id,MRCP_SESSION_ID_HEX_STRING_LENGTH,server->pool);
		apr_hash_set(server->session_table,session->id.buf,session->id.length,session);
	}
	return TRUE;
}

static apt_bool_t mrcp_server_session_terminate_process(mrcp_server_t *server, mrcp_session_t *session)
{
	apt_log(APT_PRIO_INFO,"Process Session Terminate");
	apr_hash_set(server->session_table,session->id.buf,session->id.length,NULL);
	return TRUE;
}

static apt_bool_t mrcp_server_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_server_t *server = apt_consumer_task_object_get(consumer_task);
	apt_log(APT_PRIO_DEBUG,"Process Message");
	switch(msg->type) {
		case MRCP_SERVER_SIGNALING_TASK_MSG:
		{
			sig_agent_message_t *data = (sig_agent_message_t*)msg->data;
			switch(msg->sub_type) {
				case SIG_AGENT_TASK_MSG_OFFER:
					mrcp_server_session_offer_process(server,data->session,data->descriptor);
					break;
				case SIG_AGENT_TASK_MSG_TERMINATE:
					mrcp_server_session_terminate_process(server,data->session);
					break;
				default:
					break;
			}
			break;
		}
		case MRCP_SERVER_MEDIA_TASK_MSG:
			break;
		default: 
			break;
	}
	return TRUE;
}


/* Signaling interface */
static apt_bool_t mrcp_server_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_server_session_terminate(mrcp_session_t *session);

static const mrcp_session_method_vtable_t session_method_vtable = {
	mrcp_server_session_offer,
	NULL, /* answer */
	mrcp_server_session_terminate
};

static mrcp_session_t* mrcp_server_session_create()
{
	mrcp_session_t *session = mrcp_session_create();
	session->method_vtable = &session_method_vtable;
	return session;
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
