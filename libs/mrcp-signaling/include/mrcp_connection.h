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

#ifndef __MRCP_CONNECTION_H__
#define __MRCP_CONNECTION_H__

/**
 * @file mrcp_connection.h
 * @brief Abstract MRCP Connection
 */ 

#include "mrcp_sig_types.h"

APT_BEGIN_EXTERN_C

/** MRCP connection methods vtable declaration */
typedef struct mrcp_connection_method_vtable_t mrcp_connection_method_vtable_t;
/** MRCP connection events vtable declaration */
typedef struct mrcp_connection_event_vtable_t mrcp_connection_event_vtable_t;

/** MRCP connection */
struct mrcp_connection_t {
	/** Memory pool to allocate memory from */
	apr_pool_t *pool;

	/** Virtual methods */
	const mrcp_connection_method_vtable_t *method_vtable;
	/** Virtual events */
	const mrcp_connection_event_vtable_t  *event_vtable;
};

/** MRCP connection methods vtable */
struct mrcp_connection_method_vtable_t {
	/** Disconnect existing connection if connection is no more referenced */
	apt_bool_t (*disconnect)(mrcp_connection_t *connection);
	/** Send MRCP message to peer (server/client) */
	apt_bool_t (*send)(mrcp_connection_t *connection, mrcp_message_t *mrcp_message);
};

/** MRCP connection events vtable */
struct mrcp_connection_event_vtable_t {
	/** On disconnect connection */
	apt_bool_t (*on_disconnect)(mrcp_connection_t *connection);
	/** Receive MRCP message from peer (server/client) */
	apt_bool_t (*on_receive)(mrcp_connection_t *connection, mrcp_message_t *mrcp_message);
};

APT_END_EXTERN_C

#endif /*__MRCP_CONNECTION_H__*/
