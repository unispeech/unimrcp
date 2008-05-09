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

#include "mpf_encoder.h"
#include "apt_log.h"

typedef struct mpf_encoder_t mpf_encoder_t;

struct mpf_encoder_t {
	mpf_audio_stream_t  base;
	mpf_audio_stream_t *sink;
	mpf_frame_t         encoded_frame;
};


static apt_bool_t mpf_encoder_destroy(mpf_audio_stream_t *stream)
{
	mpf_encoder_t *encoder = (mpf_encoder_t*)stream;
	return mpf_audio_stream_destroy(encoder->sink);
}

static apt_bool_t mpf_encoder_open(mpf_audio_stream_t *stream)
{
	mpf_encoder_t *encoder = (mpf_encoder_t*)stream;
	return mpf_audio_stream_tx_open(encoder->sink);
}

static apt_bool_t mpf_encoder_close(mpf_audio_stream_t *stream)
{
	mpf_encoder_t *encoder = (mpf_encoder_t*)stream;
	return mpf_audio_stream_tx_close(encoder->sink);
}

static apt_bool_t mpf_encoder_process(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	mpf_encoder_t *encoder = (mpf_encoder_t*)stream;
	if(mpf_codec_encode(encoder->sink->tx_codec,&frame->codec_frame,&encoder->encoded_frame.codec_frame) != TRUE) {
		return FALSE;
	}
	encoder->encoded_frame.event_frame = frame->event_frame;
	encoder->encoded_frame.type = frame->type;
	return mpf_audio_stream_frame_write(encoder->sink,&encoder->encoded_frame);
}


static const mpf_audio_stream_vtable_t vtable = {
	mpf_encoder_destroy,
	NULL,
	NULL,
	NULL,
	mpf_encoder_open,
	mpf_encoder_close,
	mpf_encoder_process
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_encoder_create(mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	apr_size_t frame_size;
	mpf_codec_t *codec;
	mpf_encoder_t *encoder;
	if(!sink || !sink->tx_codec) {
		return NULL;
	}
	encoder = apr_palloc(pool,sizeof(mpf_encoder_t));
	mpf_audio_stream_init(&encoder->base,&vtable);
	encoder->base.mode = STREAM_MODE_SEND;
	encoder->sink = sink;

	codec = sink->tx_codec;
	frame_size = mpf_codec_frame_size_calculate(codec->descriptor,codec->attribs);
	encoder->encoded_frame.codec_frame.size = frame_size;
	encoder->encoded_frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return &encoder->base;
}
