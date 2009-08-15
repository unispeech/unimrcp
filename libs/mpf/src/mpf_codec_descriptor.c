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

#include "mpf_codec_descriptor.h"
#include "mpf_named_event.h"
#include "mpf_rtp_pt.h"

static apt_bool_t mpf_sampling_rate_check(apr_uint16_t sampling_rate, int mask)
{
	switch(sampling_rate) {
		case 8000:
			return (mask & MPF_SAMPLE_RATE_8000) ? TRUE : FALSE;
		case 16000:
			return (mask & MPF_SAMPLE_RATE_16000) ? TRUE : FALSE;
		case 32000:
			return (mask & MPF_SAMPLE_RATE_32000) ? TRUE : FALSE;
		case 48000:
			return (mask & MPF_SAMPLE_RATE_48000) ? TRUE : FALSE;
		default:
			break;
	}
	return FALSE;
}

/** Find and return matched descriptor from codec list */
MPF_DECLARE(mpf_codec_descriptor_t*) mpf_codec_list_descriptor_find(const mpf_codec_list_t *codec_list, const mpf_codec_descriptor_t *descriptor)
{
	int i;
	mpf_codec_descriptor_t *matched_descriptor;
	for(i=0; i<codec_list->descriptor_arr->nelts; i++) {
		matched_descriptor = (mpf_codec_descriptor_t*)codec_list->descriptor_arr->elts + i;

		if(mpf_codec_descriptors_match(descriptor,matched_descriptor) == TRUE) {
			return matched_descriptor;
		}
	}
	return NULL;
}

/** Match two codec descriptors */
MPF_DECLARE(apt_bool_t) mpf_codec_descriptors_match(const mpf_codec_descriptor_t *descriptor1, const mpf_codec_descriptor_t *descriptor2)
{
	apt_bool_t match = FALSE;
	if(descriptor1->payload_type < RTP_PT_DYNAMIC && descriptor2->payload_type < RTP_PT_DYNAMIC) {
		if(descriptor1->payload_type == descriptor2->payload_type) {
			match = TRUE;
		}
	}
	else {
		if(apt_string_compare(&descriptor1->name,&descriptor2->name) == TRUE) {
			if(descriptor1->sampling_rate == descriptor2->sampling_rate && 
				descriptor1->channel_count == descriptor2->channel_count) {
				match = TRUE;
			}
		}
	}
	return match;
}

/** Match codec capabilities */
MPF_DECLARE(apt_bool_t) mpf_codec_capabilities_match(mpf_codec_descriptor_t *descriptor, const mpf_codec_descriptor_t *static_descriptor, const mpf_codec_attribs_t *attribs)
{
	apt_bool_t match = FALSE;
	if(descriptor->payload_type < RTP_PT_DYNAMIC) {
		if(static_descriptor && static_descriptor->payload_type == descriptor->payload_type) {
			descriptor->name = static_descriptor->name;
			descriptor->sampling_rate = static_descriptor->sampling_rate;
			descriptor->channel_count = static_descriptor->channel_count;
			match = TRUE;
		}
	}
	else {
		if(apt_string_compare(&attribs->name,&descriptor->name) == TRUE) {
			if(mpf_sampling_rate_check(descriptor->sampling_rate,attribs->sample_rates) == TRUE) {
				match = TRUE;
			}
		}
	}
	return match;
}

/** Intersect two codec lists */
MPF_DECLARE(apt_bool_t) mpf_codec_list_intersect(mpf_codec_list_t *codec_list1, mpf_codec_list_t *codec_list2)
{
	int i;
	mpf_codec_descriptor_t *descriptor1;
	mpf_codec_descriptor_t *descriptor2;
	codec_list1->primary_descriptor = NULL;
	codec_list1->event_descriptor = NULL;
	codec_list2->primary_descriptor = NULL;
	codec_list2->event_descriptor = NULL;
	/* find only one match for primary and named event descriptors, 
	set the matched descriptors as preffered, disable the others */
	for(i=0; i<codec_list1->descriptor_arr->nelts; i++) {
		descriptor1 = (mpf_codec_descriptor_t*)codec_list1->descriptor_arr->elts + i;

		/* check whether this is a named event descriptor */
		if(mpf_event_descriptor_check(descriptor1) == TRUE) {
			/* named event descriptor */
			if(codec_list1->event_descriptor) {
				/* named event descriptor has been already set, disable this one */
				descriptor1->enabled = FALSE;
			}
			else {
				/* find if there is a match */
				descriptor2 = mpf_codec_list_descriptor_find(codec_list2,descriptor1);
				if(descriptor2) {
					descriptor1->enabled = TRUE;
					codec_list1->event_descriptor = descriptor1;
					codec_list2->event_descriptor = descriptor2;
				}
				else {
					/* no match found, disable this descriptor */
					descriptor1->enabled = FALSE;
				}
			}
		}
		else {
			/* primary descriptor */
			if(codec_list1->primary_descriptor) {
				/* primary descriptor has been already set, disable this one */
				descriptor1->enabled = FALSE;
			}
			else {
				/* find if there is a match */
				descriptor2 = mpf_codec_list_descriptor_find(codec_list2,descriptor1);
				if(descriptor2) {
					descriptor1->enabled = TRUE;
					codec_list1->primary_descriptor = descriptor1;
					codec_list2->primary_descriptor = descriptor2;
				}
				else {
					/* no match found, disable this descriptor */
					descriptor1->enabled = FALSE;
				}
			}
		}
	}

	for(i=0; i<codec_list2->descriptor_arr->nelts; i++) {
		descriptor2 = (mpf_codec_descriptor_t*)codec_list2->descriptor_arr->elts + i;
		if(descriptor2 == codec_list2->primary_descriptor || descriptor2 == codec_list2->event_descriptor) {
			descriptor2->enabled = TRUE;
		}
		else {
			descriptor2->enabled = FALSE;
		}
	}

	return TRUE;
}
