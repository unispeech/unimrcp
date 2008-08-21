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
#include "mpf_termination.h"
#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** Plugin export defines */
#ifdef WIN32
#define MRCP_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MRCP_PLUGIN_DECLARE(type) type
#endif

/** MRCP resource engine declaration */
typedef struct mrcp_resource_engine_t mrcp_resource_engine_t;

/** Symbol name of the entry point in plugin DSO */
#define MRCP_PLUGIN_SYM_NAME "mrcp_plugin_create"
/** Prototype of resource engine creator (entry point of plugin DSO) */
typedef mrcp_resource_engine_t* (*mrcp_plugin_creator_f)(apr_pool_t *pool);

/** MRCP resource engine vtable declaration */
typedef struct mrcp_engine_method_vtable_t mrcp_engine_method_vtable_t;
/** MRCP engine channel declaration */
typedef struct mrcp_engine_channel_t mrcp_engine_channel_t;
/** MRCP engine channel virtual method table declaration */
typedef struct mrcp_engine_channel_method_vtable_t mrcp_engine_channel_method_vtable_t;
/** MRCP engine channel virtual event table declaration */
typedef struct mrcp_engine_channel_event_vtable_t mrcp_engine_channel_event_vtable_t;

/** Table of channel virtual methods */
struct mrcp_engine_channel_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_engine_channel_t *channel);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_engine_channel_t *channel);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_engine_channel_t *channel);
	/** Virtual process_request */
	apt_bool_t (*process_request)(mrcp_engine_channel_t *channel, mrcp_message_t *request);
};

/** Table of channel virtual event handlers */
struct mrcp_engine_channel_event_vtable_t {
	/** Open event handler */
	apt_bool_t (*on_open)(mrcp_engine_channel_t *channel, apt_bool_t status);
	/** Close event handler */
	apt_bool_t (*on_close)(mrcp_engine_channel_t *channel);
	/** Message event handler */
	apt_bool_t (*on_message)(mrcp_engine_channel_t *channel, mrcp_message_t *message);
};

/** MRCP engine channel declaration */
struct mrcp_engine_channel_t {
	/** Table of virtual methods */
	const mrcp_engine_channel_method_vtable_t *method_vtable;
	/** External object used with virtual methods */
	void                                      *method_obj;
	/** Table of virtual event handlers */
	const mrcp_engine_channel_event_vtable_t  *event_vtable;
	/** External object used with event handlers */
	void                                      *event_obj;
	/** Media termination */
	mpf_termination_t                         *termination;
	/** Pool to allocate memory from */
	apr_pool_t                                *pool;
};

/** Table of MRCP engine virtual methods */
struct mrcp_engine_method_vtable_t {
	/** Virtual destroy */
	apt_bool_t (*destroy)(mrcp_resource_engine_t *engine);
	/** Virtual open */
	apt_bool_t (*open)(mrcp_resource_engine_t *engine);
	/** Virtual close */
	apt_bool_t (*close)(mrcp_resource_engine_t *engine);
	/** Virtual channel create */
	mrcp_engine_channel_t* (*create_channel)(mrcp_resource_engine_t *engine, apr_pool_t *pool);
};

/** MRCP resource engine */
struct mrcp_resource_engine_t {
	/** Resource identifier */
	mrcp_resource_id                   resource_id;
	/** External object associated with engine */
	void                              *obj;
	/** Table of virtual methods */
	const mrcp_engine_method_vtable_t *method_vtable;
	/** Pool to allocate memory from */
	apr_pool_t                        *pool;
};

/** Create resource engine */
static APR_INLINE mrcp_resource_engine_t* mrcp_resource_engine_create(
								mrcp_resource_id resource_id,
								void *obj, 
								const mrcp_engine_method_vtable_t *vtable,
								apr_pool_t *pool)
{
	mrcp_resource_engine_t *engine = apr_palloc(pool,sizeof(mrcp_resource_engine_t));
	engine->resource_id = resource_id;
	engine->obj = obj;
	engine->method_vtable =vtable;
	engine->pool = pool;
	return engine;
}

/** Destroy resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_destroy(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->destroy(engine);
}

/** Open resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_open(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->open(engine);
}

/** Close resource engine */
static APR_INLINE apt_bool_t mrcp_resource_engine_close(mrcp_resource_engine_t *engine)
{
	return engine->method_vtable->close(engine);
}

/** Create engine channel */
static APR_INLINE mrcp_engine_channel_t* mrcp_engine_channel_create(
								mrcp_resource_engine_t *engine, 
								const mrcp_engine_channel_method_vtable_t *method_vtable,
								void *method_obj,
								mpf_termination_t *termination,
								apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel = apr_palloc(pool,sizeof(mrcp_engine_channel_t));
	channel->method_vtable = method_vtable;
	channel->method_obj = method_obj;
	channel->event_vtable = NULL;
	channel->event_obj = NULL;
	channel->termination = termination;
	channel->pool = pool;
	return channel;
}

/** Destroy engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_destroy(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->destroy(channel);
}

/** Open engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_open(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->open(channel);
}

/** Close engine channel */
static APR_INLINE apt_bool_t mrcp_engine_channel_close(mrcp_engine_channel_t *channel)
{
	return channel->method_vtable->close(channel);
}

/** Process request */
static APR_INLINE apt_bool_t mrcp_engine_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return channel->method_vtable->process_request(channel,message);
}

/** Send channel open response */
static APR_INLINE apt_bool_t mrcp_engine_channel_open_respond(mrcp_engine_channel_t *channel, apt_bool_t status)
{
	return channel->event_vtable->on_open(channel,status);
}

/** Send channel close response */
static APR_INLINE apt_bool_t mrcp_engine_channel_close_respond(mrcp_engine_channel_t *channel)
{
	return channel->event_vtable->on_close(channel);
}

/** Send response/event message */
static APR_INLINE apt_bool_t mrcp_engine_channel_message_send(mrcp_engine_channel_t *channel, mrcp_message_t *message)
{
	return channel->event_vtable->on_message(channel,message);
}


APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_ENGINE_H__*/
