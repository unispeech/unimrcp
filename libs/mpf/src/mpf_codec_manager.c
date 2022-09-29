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

#include <stdlib.h>
#include "mpf_codec_manager.h"
#include "mpf_rtp_pt.h"
#include "mpf_named_event.h"
#include "apt_text_stream.h"
#include "apt_log.h"


struct mpf_codec_manager_t {
	/** Memory pool */
	apr_pool_t             *pool;

	/** Dynamic (resizable) array of codecs (mpf_codec_t*) */
	apr_array_header_t     *codec_arr;
	/** Default named event descriptor */
	mpf_codec_descriptor_t *event_descriptor;
};


MPF_DECLARE(mpf_codec_manager_t*) mpf_codec_manager_create(apr_size_t codec_count, apr_pool_t *pool)
{
	mpf_codec_manager_t *codec_manager = apr_palloc(pool,sizeof(mpf_codec_manager_t));
	codec_manager->pool = pool;
	codec_manager->codec_arr = apr_array_make(pool,(int)codec_count,sizeof(mpf_codec_t*));
	codec_manager->event_descriptor = mpf_event_descriptor_create(8000,pool);
	return codec_manager;
}

MPF_DECLARE(void) mpf_codec_manager_destroy(mpf_codec_manager_t *codec_manager)
{
	/* nothing to do */
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_register(mpf_codec_manager_t *codec_manager, mpf_codec_t *codec)
{
	if(!codec || !codec->attribs || !codec->attribs->name.buf) {
		return FALSE;
	}

	apt_log(MPF_LOG_MARK,APT_PRIO_INFO,"Register Codec [%s]",codec->attribs->name.buf);

	APR_ARRAY_PUSH(codec_manager->codec_arr,mpf_codec_t*) = codec;
	return TRUE;
}

MPF_DECLARE(mpf_codec_t*) mpf_codec_manager_codec_get(const mpf_codec_manager_t *codec_manager, mpf_codec_descriptor_t *descriptor, apr_pool_t *pool)
{
	int i;
	mpf_codec_t *codec;
	if(!descriptor) {
		return NULL;
	}

	for(i=0; i<codec_manager->codec_arr->nelts; i++) {
		codec = APR_ARRAY_IDX(codec_manager->codec_arr,i,mpf_codec_t*);
		if(mpf_codec_descriptor_match_by_attribs(descriptor,codec->static_descriptor,codec->attribs) == TRUE) {
			return mpf_codec_clone(codec,pool);
		}
	}

	return NULL;
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_list_get(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, apr_pool_t *pool)
{
	const mpf_codec_descriptor_t *static_descriptor;
	mpf_codec_descriptor_t *descriptor;
	int i;
	mpf_codec_t *codec;

	mpf_codec_list_init(codec_list,codec_manager->codec_arr->nelts,pool);
	for(i=0; i<codec_manager->codec_arr->nelts; i++) {
		codec = APR_ARRAY_IDX(codec_manager->codec_arr,i,mpf_codec_t*);
		static_descriptor = codec->static_descriptor;
		if(static_descriptor) {
			descriptor = mpf_codec_list_add(codec_list);
			if(descriptor) {
				*descriptor = *static_descriptor;
			}
		}
	}
	if(codec_manager->event_descriptor) {
		descriptor = mpf_codec_list_add(codec_list);
		if(descriptor) {
			*descriptor = *codec_manager->event_descriptor;
		}
	}
	return TRUE;
}

static apt_bool_t mpf_codec_manager_codec_add(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list,
								const char *name_attr, const char *payload_type_attr,
								const char *sampling_rate_attr, const char *channel_count_attr,
								const char *format_attr, apr_pool_t *pool)
{
	mpf_codec_descriptor_t *descriptor;
	const mpf_codec_t *codec;

	if (!name_attr) {
		return FALSE;
	}

	apt_str_t name;
	apt_string_assign(&name, name_attr, pool);
	/* find codec by name */
	codec = mpf_codec_manager_codec_find(codec_manager, &name);
	if (codec) {
		descriptor = mpf_codec_list_add(codec_list);
		descriptor->name = name;
		descriptor->match_formats = codec->vtable->match_formats;

		/* set default attributes */
		if (codec->static_descriptor) {
			descriptor->payload_type = codec->static_descriptor->payload_type;
			descriptor->sampling_rate = codec->static_descriptor->sampling_rate;
			descriptor->rtp_sampling_rate = codec->static_descriptor->rtp_sampling_rate;
			descriptor->channel_count = codec->static_descriptor->channel_count;
			if (codec->static_descriptor->format_params) {
				descriptor->format_params = apt_pair_array_copy(codec->static_descriptor->format_params, pool);
			}
		}
		else {
			descriptor->payload_type = RTP_PT_DYNAMIC;
			mpf_codec_sampling_rate_set(descriptor, 8000);
			descriptor->channel_count = 1;
		}
	}
	else {
		mpf_codec_descriptor_t *event_descriptor = codec_manager->event_descriptor;
		if (event_descriptor && apt_string_compare(&event_descriptor->name, &name) == TRUE) {
			descriptor = mpf_codec_list_add(codec_list);
			*descriptor = *event_descriptor;
		}
		else {
			apt_log(MPF_LOG_MARK, APT_PRIO_WARNING, "No Such Codec [%s]", name_attr);
			return FALSE;
		}
	}

	if (payload_type_attr) {
		descriptor->payload_type = (apr_byte_t)atol(payload_type_attr);
	}

	if (sampling_rate_attr) {
		mpf_codec_sampling_rate_set(descriptor, (apr_uint16_t)atol(sampling_rate_attr));
	}

	if (channel_count_attr) {
		descriptor->channel_count = (apr_byte_t)atol(channel_count_attr);
	}

	if (format_attr) {
		apt_str_t value;
		apt_string_assign(&value, format_attr, pool);
		if (!descriptor->format_params) {
			descriptor->format_params = apt_pair_array_create(1,pool);
		}
		apt_pair_array_parse(descriptor->format_params,&value,pool);
	}

	return TRUE;
}

static apt_bool_t mpf_codec_manager_codec_parse(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, char *codec_desc_str, apr_pool_t *pool)
{
	const char *separator = "/";
	char *state;
	const char *name_attr = NULL;
	const char *payload_type_attr = NULL;
	const char *sampling_rate_attr = NULL;
	const char *channel_count_attr = NULL;
	const char *format_attr = NULL;

	/* parse codec name */
	char *str = apr_strtok(codec_desc_str, separator, &state);
	codec_desc_str = NULL; /* make sure we pass NULL on subsequent calls of apr_strtok() */
	if(str) {
		name_attr = str;

		/* parse optional payload type */
		str = apr_strtok(codec_desc_str, separator, &state);
		if(str) {
			payload_type_attr = str;

			/* parse optional sampling rate */
			str = apr_strtok(codec_desc_str, separator, &state);
			if(str) {
				sampling_rate_attr = str;

				/* parse optional channel count */
				str = apr_strtok(codec_desc_str, separator, &state);
				if(str) {
					channel_count_attr = str;

					/* parse optional format */
					str = apr_strtok(codec_desc_str, separator, &state);
					if (str) {
						format_attr = str;
					}
				}
			}
		}
	}

	return mpf_codec_manager_codec_add(codec_manager, codec_list, name_attr, payload_type_attr, sampling_rate_attr, channel_count_attr, format_attr, pool);
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_list_load(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, const char *str, apr_pool_t *pool)
{
	char *codec_desc_str;
	char *state;
	char *codec_list_str = apr_pstrdup(pool,str);
	do {
		codec_desc_str = apr_strtok(codec_list_str, " ", &state);
		if(codec_desc_str) {
			mpf_codec_manager_codec_parse(codec_manager,codec_list,codec_desc_str,pool);
		}
		codec_list_str = NULL; /* make sure we pass NULL on subsequent calls of apr_strtok() */
	} 
	while(codec_desc_str);
	return TRUE;
}

static apt_bool_t mpf_codec_manager_codec_load(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, const apr_xml_elem *elem, apr_pool_t *pool)
{
	const char *name_attr = NULL;
	const char *payload_type_attr = NULL;
	const char *sampling_rate_attr = NULL;
	const char *channel_count_attr = NULL;
	const char *format_attr = NULL;
	const char *enabled_attr = NULL;
	const apr_xml_attr *attr;
	for (attr = elem->attr; attr; attr = attr->next) {
		if (strcasecmp(attr->name,"name") == 0) {
			name_attr = attr->value;
		}
		else if (strcasecmp(attr->name,"payload-type") == 0) {
			payload_type_attr = attr->value;
		}
		if (strcasecmp(attr->name,"sampling-rate") == 0) {
			sampling_rate_attr = attr->value;
		}
		else if (strcasecmp(attr->name,"channel-count") == 0) {
			channel_count_attr = attr->value;
		}
		else if (strcasecmp(attr->name,"format") == 0) {
			format_attr = attr->value;
		}
		else if (strcasecmp(attr->name,"enable") == 0) {
			enabled_attr = attr->value;
		}
	}

	if (enabled_attr && strcasecmp(enabled_attr, "false") == 0) {
		/* skip disabled codec */
		return TRUE;
	}

	return mpf_codec_manager_codec_add(codec_manager, codec_list, name_attr, payload_type_attr, sampling_rate_attr, channel_count_attr, format_attr, pool);
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codecs_load(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Loading Codecs");
	for (elem = root->first_child; elem; elem = elem->next) {
		if (strcasecmp(elem->name, "codec") == 0) {
			mpf_codec_manager_codec_load(codec_manager, codec_list, elem, pool);
		}
		else {
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "Unknown Element <%s>", elem->name);
		}
	}
	return TRUE;
}

MPF_DECLARE(const mpf_codec_t*) mpf_codec_manager_codec_find(const mpf_codec_manager_t *codec_manager, const apt_str_t *codec_name)
{
	int i;
	mpf_codec_t *codec;
	for(i=0; i<codec_manager->codec_arr->nelts; i++) {
		codec = APR_ARRAY_IDX(codec_manager->codec_arr,i,mpf_codec_t*);
		if(apt_string_compare(&codec->attribs->name,codec_name) == TRUE) {
			return codec;
		}
	}
	return NULL;
}
