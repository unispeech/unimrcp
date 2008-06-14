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

#ifndef __MRCP_CONNECTION_TYPES_H__
#define __MRCP_CONNECTION_TYPES_H__

/**
 * @file mrcp_connection_types.h
 * @brief MRCP Connection Types Declaration
 */ 

#include <apr_network_io.h>
#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCPv2 control descriptor declaration */
typedef struct mrcp_control_descriptor_t mrcp_control_descriptor_t;

/** Opaque MRCPv2 connection declaration */
typedef struct mrcp_connection_t mrcp_connection_t;

/** Opaque MRCPv2 connection agent declaration */
typedef struct mrcp_connection_agent_t mrcp_connection_agent_t;


typedef struct mrcp_connection_event_vtable_t mrcp_connection_event_vtable_t;
struct mrcp_connection_event_vtable_t {
	apt_bool_t (*on_modify)(	mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor);
	apt_bool_t (*on_remove)(	mrcp_connection_agent_t *agent,
								void *handle);
};



APT_END_EXTERN_C

#endif /*__MRCP_CONNECTION_TYPES_H__*/
