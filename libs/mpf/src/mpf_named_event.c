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

#include "mpf_named_event.h"

#define TEL_EVENT_NAME        "telephone-event"
#define TEL_EVENT_NAME_LENGTH (sizeof(TEL_EVENT_NAME)-1)

#define TEL_EVENT_FMTP        "0-15"
#define TEL_EVENT_FMTP_LENGTH (sizeof(TEL_EVENT_FMTP)-1)


MPF_DECLARE(mpf_codec_descriptor_t*) mpf_event_descriptor_create(apr_uint16_t sampling_rate, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor = apr_palloc(pool,sizeof(mpf_codec_descriptor_t));
	mpf_codec_descriptor_init(descriptor);
	descriptor->payload_type = 101;
	descriptor->name.buf = TEL_EVENT_NAME;
	descriptor->name.length = TEL_EVENT_NAME_LENGTH;
	descriptor->sampling_rate = sampling_rate;
	descriptor->channel_count = 1;
	descriptor->format.buf = TEL_EVENT_FMTP;
	descriptor->format.length = TEL_EVENT_FMTP_LENGTH;
	return descriptor;
}

MPF_DECLARE(apt_bool_t) mpf_event_descriptor_check(const mpf_codec_descriptor_t *descriptor)
{
	apt_str_t name;
	name.buf = TEL_EVENT_NAME;
	name.length = TEL_EVENT_NAME_LENGTH;
	return apt_string_compare(&descriptor->name,&name);
}
