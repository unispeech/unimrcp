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
#include "mpf_stream.h"

typedef struct mpf_object_t mpf_object_t;

#define MAX_TERMINATION_COUNT 2

struct mpf_context_t {
	apr_pool_t        *pool;
	void              *obj;

	apr_size_t         termination_count;
	mpf_termination_t *terminations[MAX_TERMINATION_COUNT];
	mpf_object_t      *objects[MAX_TERMINATION_COUNT];
};

struct mpf_termination_t {
	apr_pool_t         *pool;
	void               *obj;

	mpf_audio_stream_t *audio_stream;
	mpf_video_stream_t *video_stream;
};

struct mpf_object_t {
	mpf_audio_stream_t *src_stream;
	mpf_audio_stream_t *dest_stream;
};

static apt_bool_t mpf_context_topology_apply(mpf_context_t *context);
static apt_bool_t mpf_context_topology_destroy(mpf_context_t *context);

MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_pool_t *pool)
{
	apr_size_t i;
	mpf_context_t *context = apr_palloc(pool,sizeof(mpf_context_t));
	context->obj = obj;
	context->pool = pool;
	context->termination_count = 0;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		context->terminations[i] = NULL;
	}

	return context;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_add(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		if(!context->terminations[i]) {
			context->terminations[i] = termination;
			context->termination_count++;
			mpf_context_topology_apply(context);
			return TRUE;
		}
	}
	return FALSE;
}

MPF_DECLARE(apt_bool_t) mpf_context_termination_remove(mpf_context_t *context, mpf_termination_t *termination)
{
	apr_size_t i;
	for(i=0; i<MAX_TERMINATION_COUNT; i++) {
		if(context->terminations[i] == termination) {
			mpf_context_topology_destroy(context);
			context->terminations[i] = NULL;
			context->termination_count--;
			return TRUE;
		}
	}
	return FALSE;
}


MPF_DECLARE(mpf_termination_t*) mpf_termination_create(void *obj, apr_pool_t *pool)
{
	mpf_termination_t *termination = apr_palloc(pool,sizeof(mpf_termination_t));
	termination->obj = obj;
	termination->pool = pool;
	termination->audio_stream = NULL;
	termination->video_stream = NULL;
	return termination;
}


static mpf_object_t* mpf_context_connection_create(mpf_context_t *context, mpf_termination_t *src_termination, mpf_termination_t *dest_termination)
{
	mpf_object_t *object = NULL;
	mpf_audio_stream_t *src_audio_stream = src_termination->audio_stream;
	mpf_audio_stream_t *dest_audio_stream = dest_termination->audio_stream;
	if(src_audio_stream && (src_audio_stream->mode & STREAM_MODE_SEND) == STREAM_MODE_SEND &&
		dest_audio_stream &&  (dest_audio_stream->mode & STREAM_MODE_RECEIVE) == STREAM_MODE_RECEIVE) {
		object = apr_palloc(context->pool,sizeof(mpf_object_t));
		object->src_stream = src_audio_stream;
		object->dest_stream = dest_audio_stream;
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
	if(context->termination_count <= 1) {
		/* at least 2 terminations are required to destroy topology */
		return TRUE;
	}
	context->objects[0] = NULL;
	context->objects[1] = NULL;
	return TRUE;
}
