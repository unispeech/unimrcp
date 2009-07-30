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
 * @brief Basic ASR engine on top of UniMRCP client library
 */ 

#include "unimrcp_client.h"

APT_BEGIN_EXTERN_C

/** Opaque ASR engine */
typedef struct asr_engine_t asr_engine_t;

/** Opaque ASR session */
typedef struct asr_session_t asr_session_t;


/**
 * Create ASR engine.
 * @param dir_layout the dir layout structure
 * @param pool the pool to allocate memory from
 */
asr_engine_t* asr_engine_create(apt_dir_layout_t *dir_layout, apr_pool_t *pool);

/**
 * Destroy ASR engine.
 * @param engine the engine to destroy
 */
apt_bool_t asr_engine_destroy(asr_engine_t *engine);



/**
 * Create ASR session.
 * @param engine the engine session belongs to
 * @param profile the name of UniMRCP profile to use
 */
asr_session_t* asr_session_create(asr_engine_t *engine, const char *profile);

/**
 * Initiate recognition.
 * @param session the session to run recognition in the scope of
 * @param grammar_file the name of the grammar file to use (path is relative to data dir)
 * @param input_file the name of the audio input file to use (path is relative to data dir)
 * @return the recognition result (input element of NLSML content)
 */
const char* asr_session_recognize(asr_session_t *session, const char *grammar_file, const char *input_file);

/**
 * Destroy ASR session.
 * @param session the session to destroy
 */
apt_bool_t asr_session_destroy(asr_session_t *session);


APT_END_EXTERN_C

#endif /*__ASR_ENGINE_H__*/
