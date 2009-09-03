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

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/** Opaque factory of media contexts */
typedef struct mpf_context_factory_t mpf_context_factory_t;
 
/**
 * Create factory of media contexts.
 */
MPF_DECLARE(mpf_context_factory_t*) mpf_context_factory_create(apr_pool_t *pool); 

/**
 * Destroy factory of media contexts.
 */
MPF_DECLARE(void) mpf_context_factory_destroy(mpf_context_factory_t *factory); 

/**
 * Process factory of media contexts.
 */
MPF_DECLARE(apt_bool_t) mpf_context_factory_process(mpf_context_factory_t *factory);

/**
 * Create MPF context.
 * @param factory the factory context belongs to
 * @param obj the external object associated with context
 * @param max_termination_count the max number of terminations in context
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_context_t*) mpf_context_create(
								mpf_context_factory_t *factory, 
								void *obj, 
								apr_size_t max_termination_count, 
								apr_pool_t *pool);

/**
 * Destroy MPF context.
 * @param context the context to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_context_destroy(mpf_context_t *context);

/**
 * Get external object associated with MPF context.
 * @param context the context to get object from
 */
MPF_DECLARE(void*) mpf_context_object_get(mpf_context_t *context);

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
 * Apply topology.
 * @param context the context which holds the termination
 * @param termination the termination to apply toplogy for
 */
MPF_DECLARE(apt_bool_t) mpf_context_topology_apply(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Destroy topology.
 * @param context the context which holds the termination
 * @param termination the termination to destroy toplogy for
 */
MPF_DECLARE(apt_bool_t) mpf_context_topology_destroy(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Process context.
 * @param context the context
 */
MPF_DECLARE(apt_bool_t) mpf_context_process(mpf_context_t *context);


APT_END_EXTERN_C

#endif /*__MPF_CONTEXT_H__*/
