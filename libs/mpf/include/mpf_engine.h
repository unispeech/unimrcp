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

#ifndef __MPF_ENGINE_H__
#define __MPF_ENGINE_H__

/**
 * @file mpf_engine.h
 * @brief Media Processing Framework Engine
 */ 

#include "apt_task.h"
#include "mpf_message.h"
#include "mpf_context.h"

APT_BEGIN_EXTERN_C

/** Opaque MPF engine declaration */
typedef struct mpf_engine_t mpf_engine_t;


/**
 * Create MPF engine.
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_engine_t*) mpf_engine_create(apr_pool_t *pool);

/**
 * Get task.
 * @param engine the engine to get task from
 */
MPF_DECLARE(apt_task_t*) mpf_task_get(mpf_engine_t *engine);


APT_END_EXTERN_C

#endif /*__MPF_ENGINE_H__*/
