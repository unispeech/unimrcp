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

#ifndef __MPF_CODEC_DESCRIPTOR_H__
#define __MPF_CODEC_DESCRIPTOR_H__

/**
 * @file mpf_codec_descriptor.h
 * @brief MPF Codec Descriptor
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

#define CODEC_FRAME_TIME_BASE 10 /*ms*/

typedef struct mpf_codec_descriptor_t mpf_codec_descriptor_t;
/** Codec descriptor */
struct mpf_codec_descriptor_t {
	apr_byte_t   payload_type;
	const char  *name;
	apr_uint16_t sampling_rate;
	apr_byte_t   channel_count;
	const char  *format;
};

typedef struct mpf_codec_frame_t mpf_codec_frame_t;
/** Codec frame */
struct mpf_codec_frame_t {
	void      *buffer;
	apr_size_t size;
};

/** Initialize codec descriptor */
static APR_INLINE void mpf_codec_descriptor_init(mpf_codec_descriptor_t *codec_descriptor)
{
	codec_descriptor->payload_type = 0;
	codec_descriptor->name = NULL;
	codec_descriptor->sampling_rate = 0;
	codec_descriptor->channel_count = 0;
	codec_descriptor->format = NULL;
}

APT_END_EXTERN_C

#endif /*__MPF_CODEC_DESCRIPTOR_H__*/
