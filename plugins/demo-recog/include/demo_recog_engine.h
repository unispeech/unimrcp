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

#ifndef __DEMO_RECOG_ENGINE_H__
#define __DEMO_RECOG_ENGINE_H__

/**
 * @file demo_recog_engine.h
 * @brief Demo Recognizer Engine
 */ 

#include "mrcp_resource_engine.h"

APT_BEGIN_EXTERN_C

/**
 * Create demo recognizer engine.
 */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) demo_recog_engine_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__DEMO_RECOG_ENGINE_H__*/
