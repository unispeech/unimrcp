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

#ifndef __MPF_STREAM_H__
#define __MPF_STREAM_H__

/**
 * @file mpf_stream.h
 * @brief MPF Bidirectional Stream
 */ 

#include "mpf_frame.h"
#include "mpf_codec.h"

APT_BEGIN_EXTERN_C

typedef enum {
	STREAM_MODE_NONE    = 0x0,
	STREAM_MODE_SEND    = 0x1,
	STREAM_MODE_RECEIVE = 0x2,

	STREAM_MODE_SEND_RECEIVE = STREAM_MODE_SEND | STREAM_MODE_RECEIVE
} mpf_stream_mode_e; 

typedef struct mpf_audio_stream_vtable_t mpf_audio_stream_vtable_t;
typedef struct mpf_audio_stream_t mpf_audio_stream_t;

struct mpf_audio_stream_t {
	const mpf_audio_stream_vtable_t *vtable;
	mpf_stream_mode_e                mode;

	mpf_codec_list_t                 codec_list;

	mpf_codec_t                     *rx_codec;
	mpf_codec_t                     *tx_codec;
};

struct mpf_audio_stream_vtable_t {
	apt_bool_t (*destroy)(mpf_audio_stream_t *stream);

	apt_bool_t (*open_rx)(mpf_audio_stream_t *stream);
	apt_bool_t (*close_rx)(mpf_audio_stream_t *stream);
	apt_bool_t (*read_frame)(mpf_audio_stream_t *stream, mpf_frame_t *frame);

	apt_bool_t (*open_tx)(mpf_audio_stream_t *stream);
	apt_bool_t (*close_tx)(mpf_audio_stream_t *stream);
	apt_bool_t (*write_frame)(mpf_audio_stream_t *stream, const mpf_frame_t *frame);
};


typedef struct mpf_video_stream_t mpf_video_stream_t;
struct mpf_video_stream_t {
	mpf_stream_mode_e mode;
};


/** Initialize audio stream */
static APR_INLINE void mpf_audio_stream_init(mpf_audio_stream_t *stream, const mpf_audio_stream_vtable_t *vtable)
{
	stream->vtable = vtable;
	stream->mode = STREAM_MODE_NONE;
	mpf_codec_list_reset(&stream->codec_list);
	stream->rx_codec = NULL;
	stream->tx_codec = NULL;
}


APT_END_EXTERN_C

#endif /*__MPF_STREAM_H__*/
