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
#include "mpf_frame.h"
#include "mpf_stream_descriptor.h"
#include "mpf_codec.h"
#include "apt_text_stream.h"

APT_BEGIN_EXTERN_C

/** Declaration of virtual table of audio stream */
typedef struct mpf_audio_stream_vtable_t mpf_audio_stream_vtable_t;

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

	/** Stream direction send/receive (bitmask of mpf_stream_direction_e) */
	mpf_stream_direction_e           direction;
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
	/** Stream direction send/receive (bitmask of mpf_stream_direction_e) */
	mpf_stream_direction_e           direction;
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

	void (*trace)(mpf_audio_stream_t *stream, mpf_stream_direction_e direction, apt_text_stream_t *output);
};

/** Create audio stream */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_stream_create(void *obj, const mpf_audio_stream_vtable_t *vtable, const mpf_stream_capabilities_t *capabilities, apr_pool_t *pool);

/** Destroy audio stream */
static APR_INLINE apt_bool_t mpf_audio_stream_destroy(mpf_audio_stream_t *stream)
{
	if(stream->vtable->destroy)
		return stream->vtable->destroy(stream);
	return TRUE;
}

/** Open audio stream receiver */
static APR_INLINE apt_bool_t mpf_audio_stream_rx_open(mpf_audio_stream_t *stream)
{
	if(stream->vtable->open_rx)
		return stream->vtable->open_rx(stream);
	return TRUE;
}

/** Close audio stream receiver */
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

/** Open audio stream transmitter */
static APR_INLINE apt_bool_t mpf_audio_stream_tx_open(mpf_audio_stream_t *stream)
{
	if(stream->vtable->open_tx)
		return stream->vtable->open_tx(stream);
	return TRUE;
}

/** Close audio stream transmitter */
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

/** Trace media path */
static APR_INLINE void mpf_audio_stream_trace(mpf_audio_stream_t *stream, mpf_stream_direction_e direction, apt_text_stream_t *output)
{
	if(stream->vtable->trace) {
		stream->vtable->trace(stream,direction,output);
		return;
	}

	if(direction & STREAM_DIRECTION_SEND) {
		mpf_codec_t *codec = stream->tx_codec;
		if(codec) {
			apr_size_t offset = output->pos - output->text.buf;
			output->pos += apr_snprintf(output->pos, output->text.length - offset,
				"[%s/%d/%d]->Sink",
				codec->descriptor->name.buf,
				codec->descriptor->sampling_rate,
				codec->descriptor->channel_count);
		}
	}
	if(direction & STREAM_DIRECTION_RECEIVE) {
		mpf_codec_t *codec = stream->rx_codec;
		if(codec) {
			apr_size_t offset = output->pos - output->text.buf;
			output->pos += apr_snprintf(output->pos, output->text.length - offset,
				"Source->[%s/%d/%d]",
				codec->descriptor->name.buf,
				codec->descriptor->sampling_rate,
				codec->descriptor->channel_count);
		}
	}
}

APT_END_EXTERN_C

#endif /*__MPF_STREAM_H__*/
