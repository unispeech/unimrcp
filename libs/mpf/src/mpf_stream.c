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

#include "mpf_stream.h"

/** Create stream capabilities */
MPF_DECLARE(mpf_stream_capabilities_t*) mpf_stream_capabilities_create(int supported_modes, apt_bool_t named_events, apr_pool_t *pool)
{
	mpf_stream_capabilities_t *capabilities = (mpf_stream_capabilities_t*)apr_palloc(pool,sizeof(mpf_stream_capabilities_t));
	capabilities->supported_modes = supported_modes;
	capabilities->named_events = named_events;
	capabilities->supported_codecs = apr_array_make(pool,1,sizeof(mpf_codec_attribs_t));
	return capabilities;
}

/** Add codec capabilities */
MPF_DECLARE(apt_bool_t) mpf_stream_capabilities_add(mpf_stream_capabilities_t *capabilities, int sample_rates, const char *codec_name, apr_pool_t *pool)
{
	mpf_codec_attribs_t *attribs = (mpf_codec_attribs_t*)apr_array_push(capabilities->supported_codecs);
	apt_string_set(&attribs->name,codec_name);
	attribs->sample_rates = sample_rates;
	attribs->bits_per_sample = 0;
	return TRUE;
}


/** Create audio stream */
MPF_DECLARE(mpf_audio_stream_t*) mpf_audio_stream_create(void *obj, const mpf_audio_stream_vtable_t *vtable, const mpf_stream_capabilities_t *capabilities, apr_pool_t *pool)
{
	mpf_audio_stream_t *stream = (mpf_audio_stream_t*)apr_palloc(pool,sizeof(mpf_audio_stream_t));
	stream->obj = obj;
	stream->vtable = vtable;
	stream->termination = NULL;
	stream->capabilities = capabilities;
	stream->mode = capabilities ? capabilities->supported_modes : STREAM_MODE_NONE;
	stream->rx_codec = NULL;
	stream->rx_event_descriptor = NULL;
	stream->tx_codec = NULL;
	stream->tx_event_descriptor = NULL;
	return stream;
}
