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

#ifndef __MPF_OBJECT_H__
#define __MPF_OBJECT_H__

/**
 * @file mpf_object.h
 * @brief Media Processing Object (bridge, multiplexor, mixer, ...)
 */ 

#include "mpf_types.h"
#include "mpf_frame.h"

APT_BEGIN_EXTERN_C

typedef struct mpf_object_t mpf_object_t;

struct mpf_object_t {
	mpf_audio_stream_t *source;
	mpf_audio_stream_t *sink;

	mpf_frame_t         frame;

	apt_bool_t (*process)(mpf_object_t *object);
	apt_bool_t (*destroy)(mpf_object_t *object);
};


APT_END_EXTERN_C

#endif /*__MPF_OBJECT_H__*/
