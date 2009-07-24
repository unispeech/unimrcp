/*
 * Copyright 2009 Arsen Chaloyan
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

#ifndef __ASR_ENGINE_H__
#define __ASR_ENGINE_H__

/**
 * @file asr_engine.h
 * @brief ASR wrapper, which uses UniMRCP client library
 */ 

#include "unimrcp_client.h"

APT_BEGIN_EXTERN_C

/** Opaque ASR engine */
typedef struct asr_engine_t asr_engine_t;

/** Opaque ASR session */
typedef struct asr_session_t asr_session_t;


/** Create ASR engine */
asr_engine_t* asr_engine_create(apt_dir_layout_t *dir_layout, apr_pool_t *pool);

/** Destroy ASR engine */
apt_bool_t asr_engine_destroy(asr_engine_t *engine);


/** Launch demo ASR session */
apt_bool_t asr_session_launch(asr_engine_t *engine, const char *grammar_file, const char *input_file, const char *profile);


APT_END_EXTERN_C

#endif /*__ASR_ENGINE_H__*/
