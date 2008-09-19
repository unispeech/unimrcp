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

#ifndef __RTSP_SERVER_CONNECTION_H__
#define __RTSP_SERVER_CONNECTION_H__

/**
 * @file rtsp_server_connection.h
 * @brief RTSP Server Connection
 */ 

#include "apt_task.h"
#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP server connection agent declaration */
typedef struct rtsp_server_agent_t rtsp_server_agent_t;
/** Opaque RTSP server connection declaration */
typedef struct rtsp_server_connection_t rtsp_server_connection_t;

/** RTSP server event vtable declaration */
typedef struct rtsp_server_agent_event_vtable_t rtsp_server_agent_event_vtable_t;

/** RTSP server event vtable */
struct rtsp_server_agent_event_vtable_t {
	/** Message receive event handler */
	apt_bool_t (*on_receive)(rtsp_server_connection_t *connection, rtsp_message_t *message);
};

/**
 * Create connection agent.
 * @param listen_ip the listen IP address
 * @param listen_port the listen port
 * @param max_connection_count the number of max RTSP connections
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_server_agent_t*) rtsp_server_connection_agent_create(
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_size_t max_connection_count,
										apr_pool_t *pool);

/**
 * Destroy connection agent.
 * @param agent the agent to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_destroy(rtsp_server_agent_t *agent);

/**
 * Start connection agent and wait for incoming requests.
 * @param agent the agent to start
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_start(rtsp_server_agent_t *agent);

/**
 * Terminate connection agent.
 * @param agent the agent to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_terminate(rtsp_server_agent_t *agent);

/**
 * Set connection event handler.
 * @param agent the agent to set event hadler for
 * @param obj the external object to associate with the agent
 * @param vtable the event handler virtual methods
 */
RTSP_DECLARE(void) rtsp_server_connection_agent_handler_set(
								rtsp_server_agent_t *agent, 
								void *obj,
								const rtsp_server_agent_event_vtable_t *vtable);

/**
 * Get task.
 * @param agent the agent to get task from
 */
RTSP_DECLARE(apt_task_t*) rtsp_server_connection_agent_task_get(rtsp_server_agent_t *agent);

/**
 * Get external object.
 * @param agent the agent to get object from
 */
RTSP_DECLARE(void*) rtsp_server_connection_agent_object_get(rtsp_server_agent_t *agent);

/**
 * Send RTSP message.
 * @param connection the connection to send message through
 * @param message the message to send
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_message_send(rtsp_server_connection_t *connection, rtsp_message_t *message);


APT_END_EXTERN_C

#endif /*__RTSP_SERVER_CONNECTION_H__*/
