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

#include "mpf_codec.h"
#include "mpf_rtp_pt.h"
#include "g722/g722.h"

#define G722_CODEC_NAME        "G722"
#define G722_CODEC_NAME_LENGTH (sizeof(G722_CODEC_NAME)-1)

typedef struct mpf_g722_encoder_t mpf_g722_encoder_t;
typedef struct mpf_g722_decoder_t mpf_g722_decoder_t;

struct mpf_g722_encoder_t {
	g722_encode_state_t   state;
	apr_int16_t          *silence_buf;
	apr_size_t            silence_buf_len;
};

struct mpf_g722_decoder_t {
	g722_decode_state_t   state;
};

static apt_bool_t mpf_g722_encoder_open(mpf_codec_t *codec, mpf_codec_descriptor_t *descriptor)
{
	mpf_g722_encoder_t *encoder = (mpf_g722_encoder_t*)apr_palloc(codec->pool, sizeof(mpf_g722_encoder_t));
	g722_encode_init(&encoder->state, 64000, 0);
	encoder->silence_buf = NULL;
	encoder->silence_buf_len = 0;

	codec->encoder_obj = encoder;
	return TRUE;
}

static apt_bool_t mpf_g722_encoder_close(mpf_codec_t *codec)
{
	mpf_g722_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	codec->encoder_obj = NULL;
	return TRUE;
}

static apt_bool_t mpf_g722_decoder_open(mpf_codec_t *codec, mpf_codec_descriptor_t *descriptor)
{
	mpf_g722_decoder_t *decoder = (mpf_g722_decoder_t*)apr_palloc(codec->pool, sizeof(mpf_g722_decoder_t));
	g722_decode_init(&decoder->state, 64000, 0);

	codec->decoder_obj = decoder;
	return TRUE;
}

static apt_bool_t mpf_g722_decoder_close(mpf_codec_t *codec)
{
	mpf_g722_decoder_t *decoder = codec->decoder_obj;
	if (!decoder)
		return FALSE;

	codec->decoder_obj = NULL;
	return TRUE;
}

static apt_bool_t mpf_g722_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	unsigned char *encode_buf;
	mpf_g722_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	encode_buf = frame_out->buffer;
	frame_out->size = g722_encode(&encoder->state, encode_buf, frame_in->buffer, frame_in->size / sizeof(apr_int16_t));
	return TRUE;
}

static apt_bool_t mpf_g722_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apr_int16_t *decode_buf;
	int size;
	mpf_g722_decoder_t *decoder = codec->decoder_obj;
	if (!decoder)
		return FALSE;

	decode_buf = frame_out->buffer;
	size = g722_decode(&decoder->state, decode_buf, frame_in->buffer, frame_in->size);
	frame_out->size = size * sizeof(apr_int16_t);
	return TRUE;
}

static apt_bool_t mpf_g722_fill(mpf_codec_t *codec, mpf_codec_frame_t *frame_out)
{
	unsigned char *encode_buf;
	mpf_g722_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	if (!encoder->silence_buf) {
		encoder->silence_buf_len = 160;
		encoder->silence_buf = (apr_int16_t*)apr_pcalloc(codec->pool, sizeof(apr_int16_t) * encoder->silence_buf_len);
	}

	encode_buf = frame_out->buffer;
	frame_out->size = g722_encode(&encoder->state, encode_buf, encoder->silence_buf, encoder->silence_buf_len);
	return TRUE;
}

static const mpf_codec_vtable_t g722_vtable = {
	mpf_g722_encoder_open,
	mpf_g722_encoder_close,
	mpf_g722_decoder_open,
	mpf_g722_decoder_close,
	mpf_g722_encode,
	mpf_g722_decode,
	NULL,
	NULL,
	mpf_g722_fill,
	NULL
};

static const mpf_codec_descriptor_t g722_descriptor = {
	RTP_PT_G722,
	{G722_CODEC_NAME, G722_CODEC_NAME_LENGTH},
	16000,
	8000,
	1,
	0,
	NULL,
	NULL,
	TRUE
};

static const mpf_codec_attribs_t g722_attribs = {
	{G722_CODEC_NAME, G722_CODEC_NAME_LENGTH},    /* codec name */
	4,                                            /* bits per sample */
	MPF_SAMPLE_RATE_16000,                        /* supported sampling rates */
	10                                            /* base frame duration */
};

mpf_codec_t* mpf_codec_g722_create(apr_pool_t *pool)
{
	return mpf_codec_create(&g722_vtable,&g722_attribs,&g722_descriptor,pool);
}
