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

#include "mpf_context.h"
#include "mpf_termination.h"
#include "mpf_stream.h"
#include "mpf_bridge.h"
#include "apt_log.h"

static apt_bool_t mpf_context_topology_apply(mpf_context_t *context);
static apt_bool_t mpf_context_topology_destroy(mpf_context_t *context);

MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_pool_t *pool)
{
	apr_size_t i;
	mpf_context_t *context = apr_palloc(pool,sizeof(mpf_context_t));
	context->obj = obj;
	context->pool = pool;
	context->elem = NULL;
	context->termination_count = 0;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		context->terminations[i] = NULL;
		context->objects[i] = NULL;
	}

	return context;
}

MPF_DECLARE(apt_bool_t) mpf_context_destroy(mpf_context_t *context)
{
	apr_size_t i;
	apr_size_t count = context->termination_count;
	mpf_termination_t *termination;
	for(i=0; i<count; i++){
		termination = context->terminations[i];
		mpf_context_termination_subtract(context,termination);
		if(termination->audio_stream) {
			termination->audio_stream->vtable->destroy(termination->audio_stream);
		}
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_add(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		if(!context->terminations[i]) {
			apt_log(APT_PRIO_INFO,"Add Termination");
			context->terminations[i] = termination;
			context->termination_count++;
			mpf_context_topology_apply(context);
			return TRUE;
		}
	}
	return FALSE;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_subtract(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		if(context->terminations[i] == termination) {
			mpf_context_topology_destroy(context);
			apt_log(APT_PRIO_INFO,"Subtract Termination");
			context->terminations[i] = NULL;
			context->termination_count--;
			return TRUE;
		}
	}
	return FALSE;
}

MPF_DECLARE(apt_bool_t) mpf_context_process(mpf_context_t *context)
{
	mpf_object_t *object;
	apr_size_t i;
	for(i=0; i<context->termination_count; i++) {
		object = context->objects[i];
		if(object && object->process) {
			object->process(object);
		}
	}
	return TRUE;
}

static mpf_object_t* mpf_context_connection_create(mpf_context_t *context, mpf_termination_t *src_termination, mpf_termination_t *sink_termination)
{
	mpf_object_t *object = NULL;
	mpf_audio_stream_t *source = src_termination->audio_stream;
	mpf_audio_stream_t *sink = sink_termination->audio_stream;
	if(source && (source->mode & STREAM_MODE_RECEIVE) == STREAM_MODE_RECEIVE &&
		sink && (sink->mode & STREAM_MODE_SEND) == STREAM_MODE_SEND) {
		object = mpf_bridge_create(source,sink,context->pool);
	}
	return object;
}

static apt_bool_t mpf_context_topology_apply(mpf_context_t *context)
{
	mpf_object_t *object;
	if(context->termination_count <= 1) {
		/* at least 2 terminations are required to apply topology on them */
		return TRUE;
	}

	if(!context->terminations[0] || !context->terminations[1]) {
		return FALSE;
	}

	object = mpf_context_connection_create(
								context,
								context->terminations[0],
								context->terminations[1]);
	if(object) {
		context->objects[0] = object;
	}

	object = mpf_context_connection_create(
								context,
								context->terminations[1],
								context->terminations[0]);
	if(object) {
		context->objects[1] = object;
	}
	return TRUE;
}

static apt_bool_t mpf_context_topology_destroy(mpf_context_t *context)
{
	mpf_object_t *object;
	if(context->termination_count <= 1) {
		/* at least 2 terminations are required to destroy topology */
		return TRUE;
	}
	object = context->objects[0];
	if(object) {
		if(object->destroy) {
			object->destroy(object);
		}
		context->objects[0] = NULL;
	}
	object = context->objects[1];
	if(object) {
		if(object->destroy) {
			object->destroy(object);
		}
		context->objects[1] = NULL;
	}
	return TRUE;
}
