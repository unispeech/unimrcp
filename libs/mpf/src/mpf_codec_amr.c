/*
 * Copyright 2022 Arsen Chaloyan
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

#include <stdlib.h>
#include "mpf_codec.h"
#include "mpf_trace.h"
#include <opencore-amrwb/dec_if.h>
#include <vo-amrwbenc/enc_if.h>

#if ENABLE_AMR_TRACE == 1
#define AMR_TRACE printf
#elif ENABLE_AMR_TRACE == 2
#define AMR_TRACE mpf_debug_output_trace
#elif ENABLE_AMR_TRACE == 3
#define AMR_TRACE(msg, args...) \
  apt_log(MPF_LOG_MARK, APT_PRIO_INFO, msg, ##args);
#else
#define AMR_TRACE mpf_null_trace
#endif

#define AMR_WB_CODEC_NAME        "AMR-WB"
#define AMR_WB_CODEC_NAME_LENGTH (sizeof(AMR_WB_CODEC_NAME)-1)

#define AMR_WB_SID  9
#define DEFAULT_AMR_WB_MODE  8

/* AMR-WB frame lengths in bytes */
static const apr_byte_t mpf_amr_wb_framelen[16] = { 17, 23, 32, 37, 40, 46, 50, 58, 60, 5, 0, 0, 0, 0, 0, 0 };

typedef struct mpf_amr_wb_encoder_t mpf_amr_wb_encoder_t;
typedef struct mpf_amr_wb_decoder_t mpf_amr_wb_decoder_t;

struct mpf_amr_wb_encoder_t {
	void          *state;
	apr_byte_t     mode;
	apt_bool_t     octet_aligned;
	apr_int16_t   *silence_buf;
	apr_size_t     silence_buf_len;
};

struct mpf_amr_wb_decoder_t {
	void        *state;
	apt_bool_t   octet_aligned;
};

static apt_bool_t mpf_amr_wb_format_params_get(const apt_pair_arr_t *format_params, apt_bool_t *octet_aligned, apr_byte_t *mode);

static apt_bool_t mpf_amr_wb_encoder_open(mpf_codec_t *codec, mpf_codec_descriptor_t *descriptor)
{
	mpf_amr_wb_encoder_t *encoder = (mpf_amr_wb_encoder_t*)apr_palloc(codec->pool, sizeof(mpf_amr_wb_encoder_t));

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "Init AMR-WB Encoder");
	encoder->state = E_IF_init();
	if (!encoder->state) {
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Failed to Init AMR-WB Encoder");
		return FALSE;
	}

	encoder->silence_buf = NULL;
	encoder->silence_buf_len = 0;
	encoder->mode = DEFAULT_AMR_WB_MODE;
	encoder->octet_aligned = FALSE;
	if (descriptor) {
		mpf_amr_wb_format_params_get(descriptor->format_params, &encoder->octet_aligned, &encoder->mode);
	}

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "AMR-WB mode [%d] octet-aligned [%d]", encoder->mode, encoder->octet_aligned);
	if (encoder->octet_aligned == FALSE) {
		/* only octet-aligned mode is currently supported */
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Only octet-aligned AMR-WB mode supported");
		E_IF_exit(encoder->state);
		encoder->state = NULL;
		return FALSE;
	}

	codec->encoder_obj = encoder;
	return TRUE;
}

static apt_bool_t mpf_amr_wb_encoder_close(mpf_codec_t *codec)
{
	mpf_amr_wb_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "Deinit AMR-WB Encoder");
	if (encoder->state) {
		E_IF_exit(encoder->state);
		encoder->state = NULL;
	}

	codec->encoder_obj = NULL;
	return TRUE;
}

static apt_bool_t mpf_amr_wb_decoder_open(mpf_codec_t *codec, mpf_codec_descriptor_t *descriptor)
{
	mpf_amr_wb_decoder_t *decoder = (mpf_amr_wb_decoder_t*)apr_palloc(codec->pool, sizeof(mpf_amr_wb_decoder_t));

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "Init AMR-WB Decoder");
	decoder->state = D_IF_init();
	if (!decoder->state) {
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Failed to Init AMR-WB Decoder");
		return FALSE;
	}

	decoder->octet_aligned = FALSE;
	if (descriptor) {
		mpf_amr_wb_format_params_get(descriptor->format_params, &decoder->octet_aligned, NULL);
	}

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "AMR-WB octet-aligned [%d]", decoder->octet_aligned);
	if (decoder->octet_aligned == FALSE) {
		/* only octet-aligned mode is currently supported */
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Only octet-aligned AMR-WB mode supported");
		D_IF_exit(decoder->state);
		decoder->state = NULL;
		return FALSE;
	}

	codec->decoder_obj = decoder;
	return TRUE;
}

static apt_bool_t mpf_amr_wb_decoder_close(mpf_codec_t *codec)
{
	mpf_amr_wb_decoder_t *decoder = codec->decoder_obj;
	if (!decoder)
		return FALSE;

	apt_log(MPF_LOG_MARK, APT_PRIO_INFO, "Deinit AMR-WB Decoder");
	if (decoder->state) {
		D_IF_exit(decoder->state);
		decoder->state = NULL;
	}

	codec->decoder_obj = NULL;
	return TRUE;
}

static apt_bool_t mpf_amr_wb_encode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	int size;
	apr_int16_t *speech;
	mpf_amr_wb_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	AMR_TRACE("AMR-WB Encode frame in [%d bytes]\n", frame_in->size);
	speech = frame_in->buffer;
	size = E_IF_encode(encoder->state, encoder->mode, speech, frame_out->buffer, 0);
	if (size <= 0) {
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Failed to AMR-WB Encode [%d]", size);
		return FALSE;
	}

	frame_out->size = size;
	AMR_TRACE("AMR-WB Encode frame out [%d bytes]\n", frame_out->size);
	return TRUE;
}

static apt_bool_t mpf_amr_wb_decode(mpf_codec_t *codec, const mpf_codec_frame_t *frame_in, mpf_codec_frame_t *frame_out)
{
	apr_int16_t *decode_buf;
	mpf_amr_wb_decoder_t *decoder = codec->decoder_obj;
	if (!decoder)
		return FALSE;

	AMR_TRACE("AMR-WB Decode frame in [%d bytes]\n", frame_in->size);
	decode_buf = frame_out->buffer;
	D_IF_decode(decoder->state, frame_in->buffer, decode_buf, 0);
	frame_out->size = 640; /* 16000 * 20 / 1000 * 2 */
	AMR_TRACE("AMR-WB Decode frame out [%d bytes]\n", frame_out->size);
	return TRUE;
}

static apt_bool_t mpf_amr_wb_pack(mpf_codec_t *codec, const mpf_codec_frame_t frames[], apr_uint16_t frame_count, apr_size_t *size)
{
	if(!frame_count)
		return FALSE;

	AMR_TRACE("AMR-WB Pack frame count [%d]\n", frame_count);
	if (frame_count == 1) {
		char* buffer = frames[0].buffer;
		memmove(buffer + 1, buffer, frames[0].size);

		/* Code Mode Request (CMR) */
		buffer[0] = 0xF0; /* 1111 0000 */
		*size += 1;
	}
	else {
		char* buffer;
		char *read_ptr;
		char *write_ptr;
		char amr_header[128];
		apr_byte_t f_bit;
		apr_byte_t frame_type;
		apr_byte_t good_quality;
		apr_byte_t toc;
		apr_size_t header_size = 0;
		const mpf_codec_frame_t *frame;
		apr_uint16_t i;

		write_ptr = &amr_header[0];

		/* Code Mode Request (CMR) */
		*write_ptr = 0xF0; /* 1111 0000 */
		write_ptr++;

		/* Table Of Contents (TOC) */
		f_bit = 1;
		for (i = 0; i < frame_count; i++) {
			read_ptr = frames[i].buffer;
			frame_type = ((*read_ptr >> 3) & 0x0F);
			good_quality = ((*read_ptr >> 2) & 0x01);

			if (i == frame_count - 1) {
				f_bit = 0;
			}

			toc = (apr_byte_t)((f_bit << 5) | (frame_type << 1) | good_quality);
			*write_ptr = (toc << 2);
			++write_ptr;
		}

		header_size = write_ptr - &amr_header[0]; /* 1 (CMR) + 1 (TOC entry) * frame count */
		buffer = frames[0].buffer;

		/* Compose payload by moving individual frames accordingly */
		for (i = frame_count; i > 0; i--) {
			frame = &frames[i-1];
			memmove((char*)frame->buffer + header_size - i, (char*)frame->buffer + 1, frame->size - 1);
		}

		/* Copy header */
		memcpy(buffer, amr_header, header_size);
		*size += header_size - frame_count;
	}

	return TRUE;
}

static apt_bool_t mpf_amr_wb_dissect(mpf_codec_t *codec, void *buffer, apr_size_t buffer_size, apr_size_t frame_size, mpf_codec_frame_t frames[], apr_uint16_t *frame_count)
{
	apr_byte_t *toc;
	apr_byte_t f_bit = 1;
	apr_byte_t frame_type;
	apr_byte_t *read_ptr = buffer;
	apr_byte_t *write_ptr = buffer;
	mpf_codec_frame_t *frame;
	apr_uint16_t cur_frame = 0;
	apr_uint16_t i;
	if (!buffer_size)
		return FALSE;

	AMR_TRACE("AMR-WB Dissect [%d bytes]\n", buffer_size);
	/* Code Mode Request (CMR) */
	read_ptr++; /* Skip CMR */
	buffer_size--;

	/* Table Of Contents (TOC) */
	toc = read_ptr;
	while (buffer_size && cur_frame < *frame_count && f_bit) {
		frame_type = ((toc[cur_frame] >> 3) & 0xF);
		f_bit = ((toc[cur_frame] >> 7) & 0x01);

		if (frame_type < AMR_WB_SID) {
			/* Speech */
		}
		else if (frame_type == AMR_WB_SID) {
			/* Silence */
		}
		else {
			/* Invalid frame type */
			return FALSE;
		}

		frame = &frames[cur_frame];
		frame->size = mpf_amr_wb_framelen[frame_type];
		AMR_TRACE("AMR-WB frame type [%d] size [%d] f bit [%d]\n", frame_type, frame->size+1, f_bit);

		cur_frame++;
		buffer_size--;
	}

	if (f_bit) {
		/* the last frame still has f_bit set */
	}

	read_ptr += cur_frame;
	*frame_count = cur_frame;

	/* Payload */
	for (i=0; i<cur_frame; i++) {
		frame = &frames[i];

		/* Copy TOC by having F bit cleared */
		*write_ptr = toc[i] & 0x7F;

		/* Align frame with TOC */
		memmove(write_ptr+1,read_ptr,frame->size);

		read_ptr += frame->size;
		frame->size++;
		frame->buffer = write_ptr;
		write_ptr += frame->size;
	}

	return TRUE;
}

static apt_bool_t mpf_amr_wb_fill(mpf_codec_t *codec, mpf_codec_frame_t *frame_out)
{
	int size;
	mpf_amr_wb_encoder_t *encoder = codec->encoder_obj;
	if (!encoder)
		return FALSE;

	AMR_TRACE("AMR-WB Fill\n");
	if (!encoder->silence_buf) {
		encoder->silence_buf_len = 160;
		encoder->silence_buf = (apr_int16_t*)apr_pcalloc(codec->pool, sizeof(apr_int16_t) * encoder->silence_buf_len);
	}

	size = E_IF_encode(encoder->state, encoder->mode, encoder->silence_buf, frame_out->buffer, 0);
	if (size <= 0) {
		apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "Failed to AMR-WB Fill [%d]", size);
		return FALSE;
	}

	frame_out->size = size;
	return TRUE;
}

static apt_bool_t mpf_amr_wb_format_params_get(const apt_pair_arr_t *format_params, apt_bool_t *octet_aligned, apr_byte_t *mode)
{
	int i;
	if (octet_aligned)
	 *octet_aligned = FALSE;
	if (mode)
		*mode = DEFAULT_AMR_WB_MODE;
	if (format_params) {
		for (i = 0; i < format_params->nelts; i++) {
			apt_pair_t* pair = &APR_ARRAY_IDX(format_params, i, apt_pair_t);
			if (pair && pair->name.buf && pair->value.buf) {
				if (octet_aligned && strcasecmp(pair->name.buf, "octet-align") == 0) {
					if (pair->value.buf[0] == '1') {
						*octet_aligned = TRUE;
					}
				}
				else if (mode && strcasecmp(pair->name.buf, "mode-set") == 0) {
					*mode = (apr_byte_t) atoi(pair->value.buf);
					if (*mode >= AMR_WB_SID) {
						*mode = DEFAULT_AMR_WB_MODE;
					}
				}
			}
		}
	}
	return TRUE;
}

static apt_bool_t mpf_amr_wb_format_match(const apt_pair_arr_t *format_params1, const apt_pair_arr_t *format_params2)
{
	apt_bool_t octet_aligned1;
	apt_bool_t octet_aligned2;
	mpf_amr_wb_format_params_get(format_params1, &octet_aligned1,NULL);
	mpf_amr_wb_format_params_get(format_params2, &octet_aligned2,NULL);

	if (octet_aligned1 != octet_aligned2) {
		return FALSE;
	}

	return TRUE;
}

static const mpf_codec_vtable_t mpf_amr_wb_vtable = {
	mpf_amr_wb_encoder_open,
	mpf_amr_wb_encoder_close,
	mpf_amr_wb_decoder_open,
	mpf_amr_wb_decoder_close,
	mpf_amr_wb_encode,
	mpf_amr_wb_decode,
	mpf_amr_wb_pack,
	mpf_amr_wb_dissect,
	mpf_amr_wb_fill,
	mpf_amr_wb_format_match
};

static const mpf_codec_attribs_t mpf_amr_wb_attribs = {
	{AMR_WB_CODEC_NAME, AMR_WB_CODEC_NAME_LENGTH},    /* codec name */
	2,                                                /* bits per sample */
	MPF_SAMPLE_RATE_16000,                            /* supported sampling rates */
	20                                                /* base frame duration */
};

mpf_codec_t* mpf_codec_amr_wb_create(apr_pool_t *pool)
{
	return mpf_codec_create(&mpf_amr_wb_vtable,&mpf_amr_wb_attribs,NULL,pool);
}
