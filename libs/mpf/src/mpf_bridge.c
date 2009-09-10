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

#include "mpf_bridge.h"
#include "mpf_encoder.h"
#include "mpf_decoder.h"
#include "mpf_resampler.h"
#include "apt_log.h"

typedef struct mpf_bridge_t mpf_bridge_t;

/** MPF bridge derived from MPF object */
struct mpf_bridge_t {
	/** MPF bridge base */
	mpf_object_t        base;
	/** Audio stream source */
	mpf_audio_stream_t *source;
	/** Audio stream sink */
	mpf_audio_stream_t *sink;

	/** Media frame used to read data from source and write it to sink */
	mpf_frame_t         frame;
};

static apt_bool_t mpf_bridge_process(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	bridge->frame.type = MEDIA_FRAME_TYPE_NONE;
	bridge->source->vtable->read_frame(bridge->source,&bridge->frame);
	
	if((bridge->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		memset(	bridge->frame.codec_frame.buffer,
				0,
				bridge->frame.codec_frame.size);
	}

	bridge->sink->vtable->write_frame(bridge->sink,&bridge->frame);
	return TRUE;
}

static apt_bool_t mpf_null_bridge_process(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	bridge->frame.type = MEDIA_FRAME_TYPE_NONE;
	bridge->source->vtable->read_frame(bridge->source,&bridge->frame);
	bridge->sink->vtable->write_frame(bridge->sink,&bridge->frame);
	return TRUE;
}


static apt_bool_t mpf_bridge_destroy(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Audio Bridge");
	mpf_audio_stream_rx_close(bridge->source);
	mpf_audio_stream_tx_close(bridge->sink);
	return TRUE;
}

static mpf_bridge_t* mpf_bridge_base_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_bridge_t *bridge;
	if(!source || !sink) {
		return NULL;
	}

	bridge = apr_palloc(pool,sizeof(mpf_bridge_t));
	bridge->source = source;
	bridge->sink = sink;
	bridge->base.process = mpf_bridge_process;
	bridge->base.destroy = mpf_bridge_destroy;

	if(mpf_audio_stream_rx_open(source) == FALSE) {
		return NULL;
	}
	if(mpf_audio_stream_tx_open(sink) == FALSE) {
		mpf_audio_stream_rx_close(source);
		return NULL;
	}
	return bridge;
}

static mpf_object_t* mpf_linear_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	apr_size_t frame_size;
	mpf_bridge_t *bridge;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Audio Bridge");
	bridge = mpf_bridge_base_create(source,sink,pool);
	if(!bridge) {
		return NULL;
	}

	descriptor = source->rx_codec->descriptor;
	frame_size = mpf_codec_linear_frame_size_calculate(descriptor->sampling_rate,descriptor->channel_count);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return &bridge->base;
}

static mpf_object_t* mpf_null_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_codec_t *codec;
	apr_size_t frame_size;
	mpf_bridge_t *bridge;
	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Audio Null Bridge");
	bridge = mpf_bridge_base_create(source,sink,pool);
	if(!bridge) {
		return NULL;
	}
	bridge->base.process = mpf_null_bridge_process;

	codec = source->rx_codec;
	frame_size = mpf_codec_frame_size_calculate(codec->descriptor,codec->attribs);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return &bridge->base;
}

MPF_DECLARE(mpf_object_t*) mpf_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_codec_t *rx_codec;
	mpf_codec_t *tx_codec;
	if(!source || !sink) {
		return NULL;
	}

	rx_codec = source->rx_codec;
	tx_codec = sink->tx_codec;
	if(!rx_codec || !tx_codec) {
		return NULL;
	}

	if(mpf_codec_descriptors_match(rx_codec->descriptor,tx_codec->descriptor) == TRUE) {
		return mpf_null_bridge_create(source,sink,pool);
	}

	if(rx_codec->vtable && rx_codec->vtable->decode) {
		/* set decoder before bridge */
		mpf_audio_stream_t *decoder = mpf_decoder_create(source,pool);
		source = decoder;
	}
	if(tx_codec->vtable && tx_codec->vtable->encode) {
		/* set encoder after bridge */
		mpf_audio_stream_t *encoder = mpf_encoder_create(sink,pool);
		sink = encoder;
	}

	if(rx_codec->descriptor->sampling_rate != tx_codec->descriptor->sampling_rate) {
		/* set resampler before bridge */
		mpf_audio_stream_t *resampler = mpf_resampler_create(source,sink,pool);
		if(!resampler) {
			return NULL;
		}
		source = resampler;
	}

	return mpf_linear_bridge_create(source,sink,pool);
}
