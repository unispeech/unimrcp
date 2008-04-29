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

APT_BEGIN_EXTERN_C

/**
 * Create MPF context.
 * @param obj the external object associated with context
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_pool_t *pool);

/**
 * Create MPF termination.
 * @param obj the external object associated with termination
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_termination_t*) mpf_termination_create(void *obj, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MPF_ENGINE_H__*/
