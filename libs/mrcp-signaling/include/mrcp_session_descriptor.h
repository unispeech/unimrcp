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

#ifndef __MRCP_SESSION_DESCRIPTOR_H__
#define __MRCP_SESSION_DESCRIPTOR_H__

/**
 * @file mrcp_session_descriptor.h
 * @brief MRCP Session Descriptor
 */ 

#include "mpf_rtp_descriptor.h"
#include "mrcp_control_descriptor.h"

APT_BEGIN_EXTERN_C

#define MAX_CONTROL_MEDIA_COUNT 3
#define MAX_AUDIO_MEDIA_COUNT   3
#define MAX_VIDEO_MEDIA_COUNT   3

/** MRCP session descriptor */
struct mrcp_session_descriptor_t {
	apt_str_t    ip;

	mrcp_control_descriptor_t  *control_descriptor[MAX_CONTROL_MEDIA_COUNT];
	apr_size_t                  control_count;
	mpf_rtp_media_descriptor_t *audio_descriptor[MAX_AUDIO_MEDIA_COUNT];
	apr_size_t                  audio_count;
	mpf_rtp_media_descriptor_t *video_descriptor[MAX_VIDEO_MEDIA_COUNT];
	apr_size_t                  video_count;
};

/** Initialize session descriptor  */
static APR_INLINE void mrcp_session_descriptor_init(mrcp_session_descriptor_t *descriptor)
{
	apt_string_reset(&descriptor->ip);
	descriptor->control_count = 0;
	descriptor->audio_count = 0;
	descriptor->video_count = 0;
}

static APR_INLINE apr_size_t mrcp_session_media_count_get(const mrcp_session_descriptor_t *descriptor)
{
	return descriptor->control_count + descriptor->audio_count + descriptor->video_count;
}

static APR_INLINE apt_bool_t mrcp_session_control_media_add(mrcp_session_descriptor_t *descriptor, mrcp_control_descriptor_t *media)
{
	if(descriptor->control_count >= MAX_CONTROL_MEDIA_COUNT) {
		return FALSE;
	}
	media->base.id = mrcp_session_media_count_get(descriptor);
	descriptor->control_descriptor[descriptor->control_count++] = media;
	return TRUE;
}

static APR_INLINE apt_bool_t mrcp_session_audio_media_add(mrcp_session_descriptor_t *descriptor, mpf_rtp_media_descriptor_t *media)
{
	if(descriptor->audio_count >= MAX_AUDIO_MEDIA_COUNT) {
		return FALSE;
	}
	media->base.id = mrcp_session_media_count_get(descriptor);
	descriptor->audio_descriptor[descriptor->audio_count++] = media;
	return TRUE;
}

static APR_INLINE apt_bool_t mrcp_session_video_media_add(mrcp_session_descriptor_t *descriptor, mpf_rtp_media_descriptor_t *media)
{
	if(descriptor->video_count >= MAX_VIDEO_MEDIA_COUNT) {
		return FALSE;
	}
	media->base.id = mrcp_session_media_count_get(descriptor);
	descriptor->video_descriptor[descriptor->video_count++] = media;
	return TRUE;
}

APT_END_EXTERN_C

#endif /*__MRCP_SESSION_DESCRIPTOR_H__*/
