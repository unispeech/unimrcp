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

#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

/**
 * @file rtsp_server.h
 * @brief RTSP Server
 */ 

#include "apt_task.h"
#include "rtsp_message.h"

APT_BEGIN_EXTERN_C

/** Opaque RTSP server declaration */
typedef struct rtsp_server_t rtsp_server_t;
/** Opaque RTSP server session declaration */
typedef struct rtsp_server_session_t rtsp_server_session_t;

/** RTSP server event vtable declaration */
typedef apt_bool_t (*rtsp_server_event_handler_f)(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message);

/**
 * Create RTSP server.
 * @param listen_ip the listen IP address
 * @param listen_port the listen port
 * @param max_connection_count the number of max RTSP connections
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_server_t*) rtsp_server_create(
								const char *listen_ip, 
								apr_port_t listen_port, 
								apr_size_t max_connection_count,
								apr_pool_t *pool);

/**
 * Destroy RTSP server.
 * @param server the server to destroy
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_destroy(rtsp_server_t *server);

/**
 * Start server and wait for incoming requests.
 * @param server the server to start
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_start(rtsp_server_t *server);

/**
 * Terminate server.
 * @param server the server to terminate
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_terminate(rtsp_server_t *server);

/**
 * Set event handler.
 * @param server the server to set event hadler for
 * @param obj the external object to associate with the server
 * @param handler the event handler
 */
RTSP_DECLARE(void) rtsp_server_event_handler_set(
								rtsp_server_t *server,
								void *obj,
								rtsp_server_event_handler_f handler);

/**
 * Get task.
 * @param server the server to get task from
 */
RTSP_DECLARE(apt_task_t*) rtsp_server_task_get(rtsp_server_t *server);

/**
 * Get external object.
 * @param server the server to get object from
 */
RTSP_DECLARE(void*) rtsp_server_object_get(rtsp_server_t *server);

/**
 * Send RTSP message.
 */
RTSP_DECLARE(apt_bool_t) rtsp_server_message_send(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message);

/**
 * Get object associated with the session.
 * @param ssession the session to get object from
 */
RTSP_DECLARE(void*) rtsp_server_session_object_get(const rtsp_server_session_t *session);

/**
 * Set object associated with the session.
 * @param session the session to set object for
 * @param obj the object to set
 */
RTSP_DECLARE(void) rtsp_server_session_object_set(rtsp_server_session_t *session, void *obj);


APT_END_EXTERN_C

#endif /*__RTSP_SERVER_H__*/
