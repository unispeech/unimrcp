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

#ifndef __MRCP_SERVER_CONNECTION_H__
#define __MRCP_SERVER_CONNECTION_H__

/**
 * @file mrcp_server_connection.h
 * @brief MRCPv2 Server Connection
 */ 

#include "apt_task.h"
#include "mrcp_connection_types.h"

APT_BEGIN_EXTERN_C

typedef struct mrcp_connection_event_vtable_t mrcp_connection_event_vtable_t;

/**
 * Create connection agent.
 * @param obj the external object to associate with the agent
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_create(
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_pool_t *pool);

/**
 * Destroy connection agent.
 * @param agent the agent to destroy
 */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_destroy(mrcp_connection_agent_t *agent);

/**
 * Start connection agent and wait for incoming requests.
 * @param agent the agent to start
 */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_start(mrcp_connection_agent_t *agent);

/**
 * Terminate connection agent.
 * @param agent the agent to terminate
 */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_terminate(mrcp_connection_agent_t *agent);

/**
 * Set connection event handler.
 * @param agent the agent to set event hadler for
 * @param obj the external object to associate with the agent
 * @param vtable the event handler virtual methods
 */
APT_DECLARE(void) mrcp_server_connection_agent_handler_set(
								mrcp_connection_agent_t *agent, 
								void *obj, 
								const mrcp_connection_event_vtable_t *vtable);

/**
 * Modify MRCPv2 connection.
 * @param agent the connection agent
 * @param handle the communication handle to response with
 * @param connection the connection (NULL on the first offer)
 * @param descriptor the control descriptor
 */
APT_DECLARE(apt_bool_t) mrcp_server_connection_modify(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor,
								apr_pool_t *pool);

/**
 * Remove MRCPv2 connection.
 * @param agent the connection agent
 * @param handle the communication handle to response with
 * @param connection the connection
 * @param descriptor the control descriptor
 */
APT_DECLARE(apt_bool_t) mrcp_server_connection_remove(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								apr_pool_t *pool);

/**
 * Get task.
 * @param agent the agent to get task from
 */
APT_DECLARE(apt_task_t*) mrcp_server_connection_agent_task_get(mrcp_connection_agent_t *agent);

/**
 * Get external object.
 * @param agent the agent to get object from
 */
APT_DECLARE(void*) mrcp_server_connection_agent_object_get(mrcp_connection_agent_t *agent);


struct mrcp_connection_event_vtable_t {
	apt_bool_t (*on_modify)(	mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor);
	apt_bool_t (*on_remove)(	mrcp_connection_agent_t *agent,
								void *handle);
};

APT_END_EXTERN_C

#endif /*__MRCP_SERVER_CONNECTION_H__*/
