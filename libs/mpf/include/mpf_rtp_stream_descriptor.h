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

#ifndef __MPF_RTP_STREAM_DESCRIPTOR_H__
#define __MPF_RTP_STREAM_DESCRIPTOR_H__

/**
 * @file mpf_rtp_stream_descriptor.h
 * @brief MPF RTP Stream Descriptor
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

typedef enum {
	RTP_MEDIA_DESCRIPTOR_NONE   = 0x0,
	RTP_MEDIA_DESCRIPTOR_LOCAL  = 0x1,
	RTP_MEDIA_DESCRIPTOR_REMOTE = 0x2
} mpf_rtp_media_descriptor_e;

typedef struct mpf_rtp_media_descriptor_t mpf_rtp_media_descriptor_t;
/** RTP media (local/remote) descriptor */
struct mpf_rtp_media_descriptor_t {
	const char      *ip;
	apr_port_t       port;

	mpf_codec_list_t codec_list;
};

typedef struct mpf_rtp_stream_descriptor_t mpf_rtp_stream_descriptor_t;
/** RTP stream descriptor */
struct mpf_rtp_stream_descriptor_t {
	mpf_stream_mode_e          mode;
	mpf_rtp_media_descriptor_e mask;
	mpf_rtp_media_descriptor_t local;
	mpf_rtp_media_descriptor_t remote;
};

typedef struct mpf_rtp_termination_descriptor_t mpf_rtp_termination_descriptor_t;
/** RTP termination descriptor */
struct mpf_rtp_termination_descriptor_t {
	mpf_rtp_stream_descriptor_t audio;
	mpf_rtp_stream_descriptor_t video;
};

static APR_INLINE void mpf_rtp_media_descriptor_init(mpf_rtp_media_descriptor_t *media)
{
	media->ip = NULL;
	media->port = 0;
	mpf_codec_list_reset(&media->codec_list);
}

static APR_INLINE void mpf_rtp_stream_descriptor_init(mpf_rtp_stream_descriptor_t *stream)
{
	stream->mode = STREAM_MODE_NONE;
	stream->mask = RTP_MEDIA_DESCRIPTOR_NONE;
	mpf_rtp_media_descriptor_init(&stream->local);
	mpf_rtp_media_descriptor_init(&stream->remote);
}

APT_END_EXTERN_C

#endif /*__MPF_RTP_STREAM_DESCRIPTOR_H__*/
