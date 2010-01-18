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

#ifndef __MRCP_SIG_AGENT_H__
#define __MRCP_SIG_AGENT_H__

/**
 * @file mrcp_sig_agent.h
 * @brief Abstract MRCP Signaling Agent
 */ 

#include <apr_network_io.h>
#include <apr_tables.h>
#include "mrcp_sig_types.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Parameters of signaling server */
struct mrcp_sig_server_params_t {
	/** Server IP address */
	char        *server_ip;
	/** Server port */
	apr_port_t   server_port;
	/** Remote SIP user name (v2 only) */
	char        *user_name;
	/** Resource location (v1 only) */
	char        *resource_location;
	/** Map of the MRCP resource names (v1 only) */
	apr_table_t *resource_map;
	/** Force destination ip address. Should be used only in case 
	SDP contains incorrect connection address (local IP address behind NAT) */
	apt_bool_t   force_destination;
};



/** MRCP signaling agent  */
struct mrcp_sig_agent_t {
	/** Memory pool to allocate memory from */
	apr_pool_t          *pool;
	/** External object associated with agent */
	void                *obj;
	/** Parent object (client/server) */
	void                *parent;
	/** MRCP version */
	mrcp_version_e       mrcp_version;
	/** MRCP resource factory */
	mrcp_resource_factory_t *resource_factory;
	/** Task interface */
	apt_task_t          *task;
	/** Task message pool used to allocate signaling agent messages */
	apt_task_msg_pool_t *msg_pool;

	/** Virtual create_server_session */
	mrcp_session_t* (*create_server_session)(mrcp_sig_agent_t *signaling_agent);
	/** Virtual create_client_session */
	apt_bool_t (*create_client_session)(mrcp_session_t *session, mrcp_sig_server_params_t *params);
};

/** Create signaling agent. */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_signaling_agent_create(void *obj, mrcp_version_e mrcp_version, apr_pool_t *pool);

/** Allocate MRCP server params. */
MRCP_DECLARE(mrcp_sig_server_params_t*) mrcp_server_params_alloc(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MRCP_SIG_AGENT_H__*/
