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

#ifndef __MPF_CODEC_H__
#define __MPF_CODEC_H__

/**
 * @file mpf_codec.h
 * @brief MPF Codec
 */ 

#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

typedef struct mpf_codec_t mpf_codec_t;
typedef struct mpf_codec_vtable_t mpf_codec_vtable_t;


/** Codec */
struct mpf_codec_t {
	const mpf_codec_descriptor_t *descriptor;
	const mpf_codec_vtable_t     *vtable;
};

/** Codec manipulator interface */
struct mpf_codec_vtable_t {
	apt_bool_t (*open)(mpf_codec_t *codec);
	apt_bool_t (*close)(mpf_codec_t *codec);

	apt_bool_t (*encode)(mpf_codec_t *codec, const codec_frame_t *frame_in, mpf_codec_frame_t *frame_out);
	apt_bool_t (*decode)(mpf_codec_t *codec, const codec_frame_t *frame_in, mpf_codec_frame_t *frame_out);

	apt_bool_t (*dissect)(mpf_codec_t *codec, void *buffer, apr_size_t size, mpf_codec_frame_t *frames, apr_size_t max_frames);
};

APT_END_EXTERN_C

#endif /*__MPF_CODEC_DESCRIPTOR_H__*/
