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

#include "mpf_types.h"
#include "mpf_stream_mode.h"
#include "mpf_frame.h"
#include "mpf_codec.h"

APT_BEGIN_EXTERN_C

/** Opaque audio stream virtual table declaration */
typedef struct mpf_audio_stream_vtable_t mpf_audio_stream_vtable_t;
/** Stream capabilities */
typedef struct mpf_stream_capabilities_t mpf_stream_capabilities_t;

/** Stream capabilities */
struct mpf_stream_capabilities_t {
	/** Supported modes: send, receive or bidirectional stream (bitmask of mpf_stream_mode_e) */
	int                 supported_modes;
	/** Supported/allowed codecs (arary of mpf_codec_attribs_t) */
	apr_array_header_t *supported_codecs;
	/** Whether stream is capable to carry named events or not */
	apt_bool_t          named_events;
};

/** Audio stream */
struct mpf_audio_stream_t {
	/** External object */
	void                            *obj;
	/** Table of virtual methods */
	const mpf_audio_stream_vtable_t *vtable;
	/** Back pointer */
	mpf_termination_t               *termination;

	/** Stream capabilities */
	const mpf_stream_capabilities_t *capabilities;

	/** Stream mode send/receive (bitmask of mpf_stream_mode_e) */
	int                              mode;
	/** Receive codec */
	mpf_codec_t                     *rx_codec;
	/** Receive event descriptor */
	mpf_codec_descriptor_t          *rx_event_descriptor;
	/** Transmit codec */
	mpf_codec_t                     *tx_codec;
	/** Transmit event descriptor */
	mpf_codec_descriptor_t          *tx_event_descriptor;
};

/** Video stream */
struct mpf_video_stream_t {
	/** Back pointer */
	mpf_termination_t               *termination;
	/** Stream mode (send/receive) */
	mpf_stream_mode_e                mode;
};

/** Table of audio stream virtual methods */
struct mpf_audio_stream_vtable_t {
	/** Virtual destroy method */
	apt_bool_t (*destroy)(mpf_audio_stream_t *stream);

	/** Virtual open receiver method */
	apt_bool_t (*open_rx)(mpf_audio_stream_t *stream);
	/** Virtual close receiver method */
	apt_bool_t (*close_rx)(mpf_audio_stream_t *stream);
	/** Virtual read frame method */
	apt_bool_t (*read_frame)(mpf_audio_stream_t *stream, mpf_frame_t *frame);

	/** Virtual open transmitter method */
	apt_bool_t (*open_tx)(mpf_audio_stream_t *stream);
	/** Virtual close transmitter method */
	apt_bool_t (*close_tx)(mpf_audio_stream_t *stream);
	/** Virtual write frame method */
	apt_bool_t (*write_frame)(mpf_audio_stream_t *stream, const mpf_frame_t *frame);
};

/** Create stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_create(int supported_modes, apt_bool_t named_events, apr_pool_t *pool);

/** Add codec capabilities */
MPF_DECLARE(apt_bool_t) mpf_stream_capabilities_add(mpf_stream_capabilities_t *capabilities, int sample_rates, const char *codec_name, apr_pool_t *pool);

/** Create audio stream */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_stream_create(void *obj, const mpf_audio_stream_vtable_t *vtable, const mpf_stream_capabilities_t *capabilities, apr_pool_t *pool);

/** Destroy audio stream */
static APR_INLINE apt_bool_t mpf_audio_stream_destroy(mpf_audio_stream_t *stream)
{
	if(stream->vtable->destroy)
		return stream->vtable->destroy(stream);
	return TRUE;
}

/** Open audio stream receive */
static APR_INLINE apt_bool_t mpf_audio_stream_rx_open(mpf_audio_stream_t *stream)
{
	if(stream->vtable->open_rx)
		return stream->vtable->open_rx(stream);
	return TRUE;
}

/** Close audio stream receive */
static APR_INLINE apt_bool_t mpf_audio_stream_rx_close(mpf_audio_stream_t *stream)
{
	if(stream->vtable->close_rx)
		return stream->vtable->close_rx(stream);
	return TRUE;
}

/** Read frame */
static APR_INLINE apt_bool_t mpf_audio_stream_frame_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	if(stream->vtable->read_frame)
		return stream->vtable->read_frame(stream,frame);
	return TRUE;
}

/** Open audio stream transmit */
static APR_INLINE apt_bool_t mpf_audio_stream_tx_open(mpf_audio_stream_t *stream)
{
	if(stream->vtable->open_tx)
		return stream->vtable->open_tx(stream);
	return TRUE;
}

/** Close audio stream transmit */
static APR_INLINE apt_bool_t mpf_audio_stream_tx_close(mpf_audio_stream_t *stream)
{
	if(stream->vtable->close_tx)
		return stream->vtable->close_tx(stream);
	return TRUE;
}

/** Write frame */
static APR_INLINE apt_bool_t mpf_audio_stream_frame_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	if(stream->vtable->write_frame)
		return stream->vtable->write_frame(stream,frame);
	return TRUE;
}

APT_END_EXTERN_C

#endif /*__MPF_STREAM_H__*/
