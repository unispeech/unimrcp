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

#include <stdlib.h>
#include "mpf_codec_manager.h"
#include "apt_log.h"


struct mpf_codec_manager_t {
	apr_pool_t   *pool;

	mpf_codec_t **codecs;
	apr_size_t    codec_count;
	apr_size_t    max_codec_count;
};


MPF_DECLARE(mpf_codec_manager_t*) mpf_codec_manager_create(apr_size_t max_codec_count, apr_pool_t *pool)
{
	size_t i;
	mpf_codec_manager_t *codec_manager = apr_palloc(pool,sizeof(mpf_codec_manager_t));
	codec_manager->pool = pool;
	codec_manager->codec_count = 0;
	codec_manager->max_codec_count = max_codec_count;
	codec_manager->codecs = apr_palloc(pool,sizeof(mpf_codec_t*)*max_codec_count);
	for(i=0; i<max_codec_count; i++) {
		codec_manager->codecs[i] = NULL;
	}

	return codec_manager;
}

MPF_DECLARE(void) mpf_codec_manager_destroy(mpf_codec_manager_t *codec_manager)
{
	/* nothing to do */
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_register(mpf_codec_manager_t *codec_manager, mpf_codec_t *codec)
{
	if(codec_manager->codec_count >= codec_manager->max_codec_count) {
		return FALSE;
	}

	if(!codec || !codec->def_descriptor || !codec->def_descriptor->name.buf) {
		return FALSE;
	}

	apt_log(APT_PRIO_INFO,"Register Codec %d [%s/%hu/%d]",
					codec->def_descriptor->payload_type,
					codec->def_descriptor->name.buf,
					codec->def_descriptor->sampling_rate,
					codec->def_descriptor->channel_count);

	codec_manager->codecs[codec_manager->codec_count] = codec;
	codec_manager->codec_count++;
	return TRUE;
}

MPF_DECLARE(mpf_codec_t*) mpf_codec_manager_codec_get(const mpf_codec_manager_t *codec_manager, mpf_codec_descriptor_t *descriptor, apr_pool_t *pool)
{
	size_t i;
	mpf_codec_t *codec = NULL;
	mpf_codec_t *ret_codec = NULL;
	if(!descriptor) {
		return NULL;
	}

	for(i=0; i<codec_manager->codec_count; i++) {
		codec = codec_manager->codecs[i];
		if(descriptor->payload_type < 96) {
			if(codec->def_descriptor->payload_type == descriptor->payload_type) {
				descriptor->name = codec->def_descriptor->name;
				descriptor->sampling_rate = codec->def_descriptor->sampling_rate;
				descriptor->channel_count = codec->def_descriptor->channel_count;
				break;
			}
		}
		else {
			if(apt_string_compare(&codec->def_descriptor->name,&descriptor->name) == TRUE) {
				break;
			}
		}
	}
	
	if(i == codec_manager->codec_count) {
		/* no match found */
		return NULL;
	}
	if(codec) {
		ret_codec = mpf_codec_clone(codec,pool);
		ret_codec->descriptor = descriptor;
	}
	return ret_codec;
}

MPF_DECLARE(apt_bool_t) mpf_codec_manager_codec_list_get(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, apr_pool_t *pool)
{
	const mpf_codec_descriptor_t *def_descriptor;
	mpf_codec_descriptor_t *descriptor;
	apr_size_t i;
	apr_size_t count = codec_manager->codec_count;
	mpf_codec_list_init(codec_list,count,pool);
	for(i=0; i<count; i++) {
		def_descriptor = codec_manager->codecs[i]->def_descriptor;
		descriptor = mpf_codec_list_add(codec_list);
		if(descriptor) {
			*descriptor = *def_descriptor;
		}
	}
	return TRUE;
}

static apt_bool_t mpf_codec_manager_codec_parse(const mpf_codec_manager_t *codec_manager, mpf_codec_list_t *codec_list, char *codec_desc_str, apr_pool_t *pool)
{
	const mpf_codec_descriptor_t *def_descriptor;
	mpf_codec_descriptor_t *descriptor;
	const char *separator = "/";
	char *state;
	/* parse codec name */
	char *str = apr_strtok(codec_desc_str, separator, &state);
	codec_desc_str = NULL; /* make sure we pass NULL on subsequent calls of apr_strtok() */
	if(str) {
		apt_str_t name;
		apt_string_assign(&name,str,pool);
		def_descriptor = mpf_codec_manager_codec_desc_find(codec_manager,&name);
		if(!def_descriptor) {
			apt_log(APT_PRIO_WARNING,"No Such Codec [%s]",str);
			return FALSE;
		}

		descriptor = mpf_codec_list_add(codec_list);
		mpf_codec_descriptor_init(descriptor);
		descriptor->name = name;

		/* parse optional payload type */
		str = apr_strtok(codec_desc_str, separator, &state);
		if(str) {
			descriptor->payload_type = (apr_byte_t)atol(str);

			/* parse optional sampling rate */
			str = apr_strtok(codec_desc_str, separator, &state);
			if(str) {
				descriptor->sampling_rate = (apr_uint16_t)atol(str);

				/* parse optional channel count */
				str = apr_strtok(codec_desc_str, separator, &state);
				if(str) {
					descriptor->channel_count = (apr_byte_t)atol(str);
				}
				else {
					/* no channel count specified */
					descriptor->channel_count = def_descriptor->channel_count;
				}
			}
			else {
				/* no sampling rate specified */
				descriptor->sampling_rate = def_descriptor->sampling_rate;
				descriptor->channel_count = def_descriptor->channel_count;
			}
		}
		else {
			/* no payload type specified */
			descriptor->payload_type = def_descriptor->payload_type;
			descriptor->sampling_rate = def_descriptor->sampling_rate;
			descriptor->channel_count = def_descriptor->channel_count;
		}
	}
	return TRUE;
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

MPF_DECLARE(const mpf_codec_descriptor_t*) mpf_codec_manager_codec_desc_find(const mpf_codec_manager_t *codec_manager, const apt_str_t *codec_name)
{
	mpf_codec_t *codec;
	size_t i;
	for(i=0; i<codec_manager->codec_count; i++) {
		codec = codec_manager->codecs[i];
		if(codec) {
			if(apt_string_compare(&codec->def_descriptor->name,codec_name) == TRUE) {
				return codec->def_descriptor;
			}
		}
	}
	return NULL;
}
