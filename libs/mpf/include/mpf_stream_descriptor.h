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

#ifndef __MPF_STREAM_DESCRIPTOR_H__
#define __MPF_STREAM_DESCRIPTOR_H__

/**
 * @file mpf_stream.h
 * @brief MPF Stream Descriptor
 */ 

#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

/** Stream capabilities declaration */
typedef struct mpf_stream_capabilities_t mpf_stream_capabilities_t;

/** Stream directions (none, send, receive, duplex) */
typedef enum {
	STREAM_DIRECTION_NONE    = 0x0, /**< none */
	STREAM_DIRECTION_SEND    = 0x1, /**< send (sink) */
	STREAM_DIRECTION_RECEIVE = 0x2, /**< receive (source) */

	STREAM_DIRECTION_DUPLEX  = STREAM_DIRECTION_SEND | STREAM_DIRECTION_RECEIVE /**< duplex */
} mpf_stream_direction_e; 


/** Stream capabilities */
struct mpf_stream_capabilities_t {
	/** Supported directions either send, receive or bidirectional stream (bitmask of mpf_stream_direction_e) */
	mpf_stream_direction_e supported_directions;
	/** Supported/allowed codecs (arary of mpf_codec_attribs_t) */
	apr_array_header_t    *supported_codecs;
	/** Whether stream is capable to carry named events or not */
	apt_bool_t             named_events;
};

/** Create stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_create(mpf_stream_direction_e supported_directions, apt_bool_t named_events, apr_pool_t *pool);

/** Add codec capabilities */
MPF_DECLARE(apt_bool_t) mpf_stream_capabilities_add(mpf_stream_capabilities_t *capabilities, int sample_rates, const char *codec_name, apr_pool_t *pool);


/** Get reverse direction */
static APR_INLINE mpf_stream_direction_e mpf_stream_reverse_direction_get(mpf_stream_direction_e direction)
{
	mpf_stream_direction_e rev_direction = direction;
	if(rev_direction == STREAM_DIRECTION_SEND) {
		rev_direction = STREAM_DIRECTION_RECEIVE;
	}
	else if(rev_direction == STREAM_DIRECTION_RECEIVE) {
		rev_direction = STREAM_DIRECTION_SEND;
	}
	return rev_direction;
}


APT_END_EXTERN_C

#endif /*__MPF_STREAM_DESCRIPTOR_H__*/
