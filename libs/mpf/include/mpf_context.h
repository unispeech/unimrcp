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

#ifndef __MPF_CONTEXT_H__
#define __MPF_CONTEXT_H__

/**
 * @file mpf_context.h
 * @brief MPF Context
 */ 

#include "mpf_message.h"
#include "mpf_object.h"
#include "apt_obj_list.h"

APT_BEGIN_EXTERN_C

#define MAX_TERMINATION_COUNT 2

struct mpf_context_t {
	apr_pool_t        *pool;
	void              *obj;
	apt_list_elem_t   *elem;

	apr_size_t         termination_count;
	mpf_termination_t *terminations[MAX_TERMINATION_COUNT];
	mpf_object_t      *objects[MAX_TERMINATION_COUNT];
};

/**
 * Create MPF context.
 * @param obj the external object associated with context
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_pool_t *pool);

/**
 * Destroy MPF context.
 * @param context the context to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_context_destroy(mpf_context_t *context);

/**
 * Add termination to context.
 * @param context the context to add termination to
 * @param termination the termination to add
 */
MPF_DECLARE(apt_bool_t) mpf_context_termination_add(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Subtract termination from context.
 * @param context the context to subtract termination from
 * @param termination the termination to subtract
 */
MPF_DECLARE(apt_bool_t) mpf_context_termination_subtract(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Process context.
 * @param context the context
 */
MPF_DECLARE(apt_bool_t) mpf_context_process(mpf_context_t *context);


/**
 * Create MPF termination.
 * @param obj the external object associated with termination
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_termination_t*) mpf_termination_create(
										void *obj, 
										mpf_audio_stream_t *audio_stream, 
										mpf_video_stream_t *video_stream, 
										apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MPF_ENGINE_H__*/
