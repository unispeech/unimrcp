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

#ifndef __MRCP_RESOURCE_ENGINE_H__
#define __MRCP_RESOURCE_ENGINE_H__

/**
 * @file mrcp_resource_engine.h
 * @brief MRCP Resource Engine Interface
 */ 

#include "mrcp_types.h"
#include "mpf_types.h"

APT_BEGIN_EXTERN_C

#ifdef WIN32
#define MRCP_PLUGIN_DECLARE(type)   __declspec(dllexport) type __stdcall
#else
#define MRCP_PLUGIN_DECLARE(type) type
#endif

/** MRCP resource engine declaration */
typedef struct mrcp_resource_engine_t mrcp_resource_engine_t;
typedef struct mrcp_engine_method_vtable_t mrcp_engine_method_vtable_t;

/** MRCP engine channel declaration */
typedef struct mrcp_engine_channel_t mrcp_engine_channel_t;
typedef struct mrcp_engine_channel_method_vtable_t mrcp_engine_channel_method_vtable_t;
typedef struct mrcp_engine_channel_event_vtable_t mrcp_engine_channel_event_vtable_t;

struct mrcp_engine_channel_method_vtable_t {
	apt_bool_t (*open)(mrcp_engine_channel_t *channel);
	apt_bool_t (*request_process)(mrcp_engine_channel_t *channel, mrcp_message_t *request);
	apt_bool_t (*close)(mrcp_engine_channel_t *channel);
};

struct mrcp_engine_channel_event_vtable_t {
	apt_bool_t (*on_open)(mrcp_engine_channel_t *channel, apt_bool_t status);
	apt_bool_t (*on_message)(mrcp_engine_channel_t *channel, mrcp_message_t *message);
	apt_bool_t (*on_close)(mrcp_engine_channel_t *channel);
};

struct mrcp_engine_channel_t {
	const mrcp_engine_channel_method_vtable_t *method_vtable;
	void                                      *method_obj;
	const mrcp_engine_channel_event_vtable_t  *event_vtable;
	void                                      *event_obj;
	mpf_termination_t                         *termination;
	apr_pool_t                                *pool;
};


struct mrcp_engine_method_vtable_t {
	apt_bool_t (*open)(mrcp_resource_engine_t *engine);
	apt_bool_t (*close)(mrcp_resource_engine_t *engine);
	apt_bool_t (*destroy)(mrcp_resource_engine_t *engine);
};

struct mrcp_resource_engine_t {
	const mrcp_engine_method_vtable_t *method_vtable;
	void                              *obj;
	apr_pool_t                        *pool;
};

APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_ENGINE_H__*/
