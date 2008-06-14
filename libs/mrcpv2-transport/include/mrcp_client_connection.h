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

#ifndef __MRCPV2_CLIENT_AGENT_H__
#define __MRCPV2_CLIENT_AGENT_H__

/**
 * @file mrcpv2_client_agent.h
 * @brief MRCPv2 Client Connection Agent
 */ 

#include "mrcp_types.h"
#include "mrcp_connection_types.h"

APT_BEGIN_EXTERN_C

/**
 * Create connection agent.
 * @param obj the external object to associate with the agent
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(mrcp_connection_agent_t*) mrcpv2_client_agent_create(void *obj, apr_pool_t *pool);

/**
 * Destroy connection agent.
 * @param agent the agent to destroy
 */
APT_DECLARE(apt_bool_t) mrcpv2_client_agent_destroy(mrcp_connection_agent_t *agent);

/**
 * Start connection agent and wait for incoming requests.
 * @param agent the agent to start
 */
APT_DECLARE(apt_bool_t) mrcpv2_client_agent_start(mrcp_connection_agent_t *agent);

/**
 * Terminate connection agent.
 * @param agent the agent to terminate
 */
APT_DECLARE(apt_bool_t) mrcpv2_client_agent_terminate(mrcp_connection_agent_t *agent);


APT_END_EXTERN_C

#endif /*__MRCPV2_CLIENT_AGENT_H__*/
