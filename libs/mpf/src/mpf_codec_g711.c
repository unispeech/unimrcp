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

#include "mpf_codec.h"
#include "g711/g711.h"

static apt_bool_t g711_open(mpf_codec_t *codec)
{
	return APR_SUCCESS;
}

static apt_bool_t g711_close(mpf_codec_t *codec)
{
	return APR_SUCCESS;
}

static apt_bool_t g711u_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	const short *decode_buf;
	unsigned char *encode_buf;
	apr_uint32_t i;

	decode_buf = frame_in->buffer;
	encode_buf = frame_out->buffer;

	frame_out->size = frame_in->size / sizeof(short);

	for(i=0; i<frame_out->size; i++) {
		encode_buf[i] = linear_to_ulaw(decode_buf[i]);
	}

	return APR_SUCCESS;
}

static apt_bool_t g711u_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	short *decode_buf;
	const unsigned char *encode_buf;
	apr_uint32_t i;

	decode_buf = frame_out->buffer;
	encode_buf = frame_in->buffer;

	frame_out->size = frame_in->size * sizeof(short);

	for(i=0; i<frame_in->size; i++) {
		decode_buf[i] = ulaw_to_linear(encode_buf[i]);
	}

	return APR_SUCCESS;
}

static apt_bool_t g711a_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	const short *decode_buf;
	unsigned char *encode_buf;
	apr_uint32_t i;

	decode_buf = frame_in->buffer;
	encode_buf = frame_out->buffer;

	frame_out->size = frame_in->size / sizeof(short);

	for(i=0; i<frame_out->size; i++) {
		encode_buf[i] = linear_to_alaw(decode_buf[i]);
	}

	return APR_SUCCESS;
}

static apt_bool_t g711a_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	short *decode_buf;
	const unsigned char *encode_buf;
	apr_uint32_t i;

	decode_buf = frame_out->buffer;
	encode_buf = frame_in->buffer;

	frame_out->size = frame_in->size * sizeof(short);

	for(i=0; i<frame_in->size; i++) {
		decode_buf[i] = alaw_to_linear(encode_buf[i]);
	}

	return APR_SUCCESS;
}

static mpf_codec_vtable_t g711u_vtable = {
	g711_open,
	g711_close,
	g711u_encode,
	g711u_decode,
	NULL
};

static mpf_codec_vtable_t g711a_vtable = {
	g711_open,
	g711_close,
	g711a_encode,
	g711a_decode,
	NULL
};

static mpf_codec_descriptor_t g711u_descriptor = {
	0,
	"PCMU",
	8000,
	1,
	NULL
};

static mpf_codec_descriptor_t g711a_descriptor = {
	8,
	"PCMA",
	8000,
	1,
	NULL
};

static mpf_codec_attribs_t g711_attribs = {
	8 /* bits per sample */
};

mpf_codec_t* mpf_codec_g711u_create(apr_pool_t *pool)
{
	return mpf_codec_create(&g711u_vtable,&g711_attribs,&g711u_descriptor,pool);
}

mpf_codec_t* mpf_codec_g711a_create(apr_pool_t *pool)
{
	return mpf_codec_create(&g711a_vtable,&g711_attribs,&g711a_descriptor,pool);
}
