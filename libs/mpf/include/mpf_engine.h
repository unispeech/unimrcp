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

#include "mpf_message.h"

APT_BEGIN_EXTERN_C

/** Opaque MPF engine declaration */
typedef struct mpf_engine_t mpf_engine_t;


/**
 * Create MPF engine.
 * @param master_task the master task to send responses and events to
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_engine_t*) mpf_engine_create(
									apt_composite_task_t *master_task,
									apr_pool_t *pool);

/**
 * Get composite task.
 * @param engine the engine to get composite task from
 */
MPF_DECLARE(apt_composite_task_t*) mpf_task_get(mpf_engine_t *engine);


APT_END_EXTERN_C

#endif /*__MPF_ENGINE_H__*/
