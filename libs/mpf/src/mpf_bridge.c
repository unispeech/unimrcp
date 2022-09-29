/*
 * Copyright 2008-2015 Arsen Chaloyan
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
#include "mpf_codec_manager.h"
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
	/** Codec used in case of null bridge */
	mpf_codec_t        *codec;
	/** Media frame used to read data from source and write it to sink */
	mpf_frame_t         frame;
	/** Number of ticks in a frame (frame_duration/CODEC_FRAME_TIME_BASE) */
	apr_byte_t          base_ticks;
	/** Number of ticks incremented on every CODEC_FRAME_TIME_BASE */
	apr_byte_t          cur_ticks;
};

static APR_INLINE apt_bool_t mpf_bridge_ticks_check(mpf_bridge_t *bridge)
{
	if (bridge->base_ticks > 1) {
		bridge->cur_ticks++;
		if (bridge->cur_ticks < bridge->base_ticks) {
			return FALSE;
		}
		bridge->cur_ticks = 0;
	}
	return TRUE;
}

static apt_bool_t mpf_bridge_process(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	if (mpf_bridge_ticks_check(bridge) == FALSE)
		return TRUE;
	bridge->frame.type = MEDIA_FRAME_TYPE_NONE;
	bridge->frame.marker = MPF_MARKER_NONE;
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
	if (mpf_bridge_ticks_check(bridge) == FALSE)
		return TRUE;
	bridge->frame.type = MEDIA_FRAME_TYPE_NONE;
	bridge->frame.marker = MPF_MARKER_NONE;
	bridge->source->vtable->read_frame(bridge->source,&bridge->frame);

	if((bridge->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		/* generate silence frame */
		mpf_codec_fill(bridge->codec,&bridge->frame.codec_frame);
	}

	bridge->sink->vtable->write_frame(bridge->sink,&bridge->frame);
	return TRUE;
}

static void mpf_bridge_trace(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	char buf[1024];
	apr_size_t offset;

	apt_text_stream_t output;
	apt_text_stream_init(&output,buf,sizeof(buf)-1);

	mpf_audio_stream_trace(bridge->source,STREAM_DIRECTION_RECEIVE,&output);
	
	offset = output.pos - output.text.buf;
	output.pos += apr_snprintf(output.pos, output.text.length - offset,
		"->Bridge->");

	mpf_audio_stream_trace(bridge->sink,STREAM_DIRECTION_SEND,&output);

	*output.pos = '\0';
	apt_log(MPF_LOG_MARK,APT_PRIO_INFO,"Media Path %s %s",object->name,output.text.buf);
}


static apt_bool_t mpf_bridge_destroy(mpf_object_t *object)
{
	mpf_bridge_t *bridge = (mpf_bridge_t*) object;
	apt_log(MPF_LOG_MARK,APT_PRIO_DEBUG,"Destroy Audio Bridge %s",object->name);
	mpf_audio_stream_rx_close(bridge->source);
	mpf_audio_stream_tx_close(bridge->sink);
	return TRUE;
}

static mpf_bridge_t* mpf_bridge_base_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_byte_t base_ticks, const char *name, apr_pool_t *pool)
{
	mpf_bridge_t *bridge;
	if(!source || !sink) {
		return NULL;
	}

	bridge = apr_palloc(pool,sizeof(mpf_bridge_t));
	bridge->source = source;
	bridge->sink = sink;
	bridge->codec = NULL;
	bridge->base_ticks = base_ticks;
	bridge->cur_ticks = 0;
	mpf_object_init(&bridge->base,name);
	bridge->base.destroy = mpf_bridge_destroy;
	bridge->base.process = mpf_bridge_process;
	bridge->base.trace = mpf_bridge_trace;
	return bridge;
}

static mpf_object_t* mpf_linear_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, const mpf_codec_manager_t *codec_manager, const char *name, apr_uint16_t frame_duration, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	apr_size_t frame_size;
	mpf_bridge_t *bridge;
	apt_log(MPF_LOG_MARK,APT_PRIO_DEBUG,"Create Linear Audio Bridge %s",name);
	bridge = mpf_bridge_base_create(source,sink,(apr_byte_t)frame_duration/CODEC_FRAME_TIME_BASE,name,pool);
	if(!bridge) {
		return NULL;
	}

	descriptor = source->rx_descriptor;
	frame_size = mpf_codec_linear_frame_size_calculate(descriptor->sampling_rate,descriptor->channel_count,frame_duration);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	
	if(mpf_audio_stream_rx_open(source,NULL) == FALSE) {
		return NULL;
	}
	if(mpf_audio_stream_tx_open(sink,NULL) == FALSE) {
		mpf_audio_stream_rx_close(source);
		return NULL;
	}
	return &bridge->base;
}

static mpf_object_t* mpf_null_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, const mpf_codec_manager_t *codec_manager, const char *name, apr_pool_t *pool)
{
	mpf_codec_t *codec;
	apr_size_t frame_size;
	apr_uint16_t frame_duration;
	mpf_bridge_t *bridge;
	apt_log(MPF_LOG_MARK,APT_PRIO_DEBUG,"Create Null Audio Bridge %s",name);

	codec = mpf_codec_manager_codec_get(codec_manager,source->rx_descriptor,pool);
	if (!codec) {
		return NULL;
	}

	frame_duration = codec->attribs->frame_duration;
	bridge = mpf_bridge_base_create(source,sink,(apr_byte_t)frame_duration/CODEC_FRAME_TIME_BASE,name,pool);
	if(!bridge) {
		return NULL;
	}
	bridge->base.process = mpf_null_bridge_process;

	source->rx_descriptor->frame_duration = frame_duration;
	sink->tx_descriptor->frame_duration = frame_duration;
	frame_size = mpf_codec_frame_size_calculate(
		source->rx_descriptor->sampling_rate,
		source->rx_descriptor->channel_count,
		source->rx_descriptor->frame_duration,
		codec->attribs->bits_per_sample);
	bridge->codec = codec;
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);

	if(mpf_audio_stream_rx_open(source,codec) == FALSE) {
		return NULL;
	}
	if(mpf_audio_stream_tx_open(sink,codec) == FALSE) {
		mpf_audio_stream_rx_close(source);
		return NULL;
	}
	return &bridge->base;
}

MPF_DECLARE(mpf_object_t*) mpf_bridge_create(
						mpf_audio_stream_t *source, 
						mpf_audio_stream_t *sink, 
						const mpf_codec_manager_t *codec_manager, 
						const char *name,
						apr_pool_t *pool)
{
	apr_uint16_t frame_duration = CODEC_FRAME_TIME_BASE;
	mpf_codec_t *source_codec = NULL;
	mpf_codec_t *sink_codec = NULL;
	if(!source || !sink) {
		return NULL;
	}

	if(mpf_audio_stream_rx_validate(source,sink->tx_descriptor,sink->tx_event_descriptor,pool) == FALSE ||
		mpf_audio_stream_tx_validate(sink,source->rx_descriptor,source->rx_event_descriptor,pool) == FALSE) {
		return NULL;
	}

	if(mpf_codec_descriptors_match(source->rx_descriptor,sink->tx_descriptor) == TRUE) {
		return mpf_null_bridge_create(source,sink,codec_manager,name,pool);
	}

	if(mpf_codec_lpcm_descriptor_match(source->rx_descriptor) == FALSE) {
		source_codec = mpf_codec_manager_codec_get(codec_manager,source->rx_descriptor,pool);
		if(source_codec) {
			if (source_codec->attribs->frame_duration > frame_duration) {
				frame_duration = source_codec->attribs->frame_duration;
			}
		}
	}

	if(mpf_codec_lpcm_descriptor_match(sink->tx_descriptor) == FALSE) {
		sink_codec = mpf_codec_manager_codec_get(codec_manager,sink->tx_descriptor,pool);
		if (sink_codec) {
			if (sink_codec->attribs->frame_duration > frame_duration) {
				frame_duration = sink_codec->attribs->frame_duration;
			}
		}
	}

	source->rx_descriptor->frame_duration = frame_duration;
	sink->tx_descriptor->frame_duration = frame_duration;

	if (source_codec) {
		/* set decoder before bridge */
		mpf_audio_stream_t *decoder = mpf_decoder_create(source, source_codec, pool);
		source = decoder;
	}

	if (sink_codec) {
		/* set encoder after bridge */
		mpf_audio_stream_t *encoder = mpf_encoder_create(sink, sink_codec, pool);
		sink = encoder;
	}

	if(source->rx_descriptor->sampling_rate != sink->tx_descriptor->sampling_rate) {
		/* set resampler before bridge */
		mpf_audio_stream_t *resampler = mpf_resampler_create(source,sink,pool);
		if(!resampler) {
			return NULL;
		}
		source = resampler;
	}

	return mpf_linear_bridge_create(source,sink,codec_manager,name,frame_duration,pool);
}
