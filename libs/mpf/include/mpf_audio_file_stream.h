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

#ifndef __MPF_AUDIO_FILE_STREAM_H__
#define __MPF_AUDIO_FILE_STREAM_H__

/**
 * @file mpf_audio_file_stream.h
 * @brief MPF Audio FIle Stream
 */ 

#include "mpf_stream.h"

APT_BEGIN_EXTERN_C

/** Opaque audio file stream declaration */
typedef struct mpf_audio_file_stream_t mpf_audio_file_stream_t;

/**
 * Create audio file reader stream.
 * @param file_name the file name (path) to stream audio from
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_file_reader_create(const char *file_name, apr_pool_t *pool);

/**
 * Create audio file writer stream.
 * @param file_name the file name (path) to stream audio to
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_file_writer_create(const char *file_name, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MPF_AUDIO_FILE_STREAM_H__*/
