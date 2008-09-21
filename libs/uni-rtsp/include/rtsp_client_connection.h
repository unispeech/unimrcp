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

#ifndef __RTSP_CLIENT_CONNECTION_H__
#define __RTSP_CLIENT_CONNECTION_H__

/**
 * @file rtso_client_connection.h
 * @brief RTSP Client Connection
 */ 

#include "apt_task.h"
#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP client connection agent declaration */
typedef struct rtsp_client_agent_t rtsp_client_agent_t;
/** Opaque RTSP client connection declaration */
typedef struct rtsp_client_connection_t rtsp_client_connection_t;

/** RTSP client event vtable declaration */
typedef struct rtsp_client_agent_event_vtable_t rtsp_client_agent_event_vtable_t;

/** RTSP client event vtable */
struct rtsp_client_agent_event_vtable_t {
	/** On connection accept */
	apt_bool_t (*on_connect)(rtsp_client_connection_t *connection);
	/** On connection disconnect */
	apt_bool_t (*on_disconnect)(rtsp_client_connection_t *connection);

	/** Message receive event handler */
	apt_bool_t (*on_receive)(rtsp_client_connection_t *connection, rtsp_message_t *message);
};


/**
 * Create connection agent.
 * @param max_connection_count the number of max RTSP connections
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_client_agent_t*) rtsp_client_connection_agent_create(
										const char *server_ip,
										apr_port_t server_port,
										apr_size_t max_connection_count, 
										apr_pool_t *pool);

/**
 * Destroy connection agent.
 * @param agent the agent to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_connection_agent_destroy(rtsp_client_agent_t *agent);

/**
 * Start connection agent and wait for incoming requests.
 * @param agent the agent to start
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_connection_agent_start(rtsp_client_agent_t *agent);

/**
 * Terminate connection agent.
 * @param agent the agent to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_connection_agent_terminate(rtsp_client_agent_t *agent);


/**
 * Set connection event handler.
 * @param agent the agent to set event hadler for
 * @param obj the external object to associate with the agent
 * @param vtable the event handler virtual methods
 */
RTSP_DECLARE(void) rtsp_client_connection_agent_handler_set(
								rtsp_client_agent_t *agent, 
								void *obj, 
								const rtsp_client_agent_event_vtable_t *vtable);

/**
 * Get task.
 * @param agent the agent to get task from
 */
RTSP_DECLARE(apt_task_t*) rtsp_client_connection_agent_task_get(rtsp_client_agent_t *agent);

/**
 * Get external object.
 * @param agent the agent to get object from
 */
RTSP_DECLARE(void*) rtsp_client_connection_agent_object_get(rtsp_client_agent_t *agent);


/**
 * Create RTSP connection.
 * @param agent the agent to create connection for
 */
RTSP_DECLARE(rtsp_client_connection_t*) rtsp_client_connection_create(rtsp_client_agent_t *agent);

/**
 * Destroy RTSP connection.
 * @param agent the agent to destroy connection for
 * @param connection the connection to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_connection_destroy(rtsp_client_agent_t *agent, rtsp_client_connection_t *connection);

/**
 * Send RTSP message.
 * @param connection the connection to send message through
 * @param message the message to send
 */
RTSP_DECLARE(apt_bool_t) rtsp_client_message_send(rtsp_client_agent_t *agent, rtsp_client_connection_t *connection, rtsp_message_t *message);

APT_END_EXTERN_C

#endif /*__RTSP_CLIENT_CONNECTION_H__*/
