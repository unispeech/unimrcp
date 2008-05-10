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

#ifndef __MPF_CODEC_H__
#define __MPF_CODEC_H__

/**
 * @file mpf_codec.h
 * @brief MPF Codec
 */ 

#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

typedef struct mpf_codec_t mpf_codec_t;
typedef struct mpf_codec_vtable_t mpf_codec_vtable_t;


/** Codec */
struct mpf_codec_t {
	const mpf_codec_vtable_t     *vtable;
	const mpf_codec_attribs_t    *attribs;
	const mpf_codec_descriptor_t *def_descriptor;
	
	mpf_codec_descriptor_t       *descriptor;
};

/** Codec manipulator interface */
struct mpf_codec_vtable_t {
	apt_bool_t (*open)(mpf_codec_t *codec);
	apt_bool_t (*close)(mpf_codec_t *codec);

	apt_bool_t (*encode)(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out);
	apt_bool_t (*decode)(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out);

	apt_bool_t (*dissect)(mpf_codec_t *codec, void **buffer, apr_size_t *size, mpf_codec_frame_t *frame);
};

static APR_INLINE mpf_codec_t* mpf_codec_create(
									const mpf_codec_vtable_t *vtable, 
									const mpf_codec_attribs_t *attribs, 
									const mpf_codec_descriptor_t *descriptor, 
									apr_pool_t *pool)
{
	mpf_codec_t *codec = apr_palloc(pool,sizeof(mpf_codec_t));
	codec->vtable = vtable;
	codec->attribs = attribs;
	codec->def_descriptor = descriptor;
	codec->descriptor = NULL;
	return codec;
}

static APR_INLINE mpf_codec_t* mpf_codec_clone(mpf_codec_t *src_codec, apr_pool_t *pool)
{
	mpf_codec_t *codec = apr_palloc(pool,sizeof(mpf_codec_t));
	codec->vtable = src_codec->vtable;
	codec->attribs = src_codec->attribs;
	codec->def_descriptor = src_codec->def_descriptor;
	codec->descriptor = src_codec->descriptor;
	return codec;
}

static APR_INLINE apt_bool_t mpf_codec_open(mpf_codec_t *codec)
{
	apt_bool_t rv = TRUE;
	if(codec->descriptor) {
		if(codec->vtable->open) {
			rv = codec->vtable->open(codec);
		}
	}
	else {
		rv = FALSE;
	}
	return rv;
}

static APR_INLINE apt_bool_t mpf_codec_close(mpf_codec_t *codec)
{
	apt_bool_t rv = TRUE;
	if(codec->vtable->close) {
		rv = codec->vtable->close(codec);
	}
	return rv;
}

static APR_INLINE apt_bool_t mpf_codec_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apt_bool_t rv = TRUE;
	if(codec->vtable->encode) {
		rv = codec->vtable->encode(codec,frame_in,frame_out);
	}
	return rv;
}

static APR_INLINE apt_bool_t mpf_codec_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apt_bool_t rv = TRUE;
	if(codec->vtable->decode) {
		rv = codec->vtable->decode(codec,frame_in,frame_out);
	}
	return rv;
}

static APR_INLINE apt_bool_t mpf_codec_dissect(mpf_codec_t *codec, void **buffer, apr_size_t *size, mpf_codec_frame_t *frame)
{
	apt_bool_t rv = TRUE;
	if(codec->vtable->dissect) {
		/* custom dissector for codecs like G.729, G.723 */
		rv = codec->vtable->dissect(codec,buffer,size,frame);
	}
	else {
		/* default dissector */
		if(*size >= frame->size && frame->size) {
			memcpy(frame->buffer,buffer,frame->size);
			
			*buffer = (char*)(*buffer) + frame->size;
			*size -= frame->size;
		}
		else {
			rv = FALSE;
		}
	}
	return rv;
}

APT_END_EXTERN_C

#endif /*__MPF_CODEC_H__*/
