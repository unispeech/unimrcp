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

#include "mpf_termination.h"
#include "mpf_stream.h"

MPF_DECLARE(mpf_termination_t*) mpf_termination_create(
										void *obj,
										const mpf_termination_vtable_t *vtable,
										mpf_audio_stream_t *audio_stream,
										mpf_video_stream_t *video_stream,
										apr_pool_t *pool)
{
	mpf_termination_t *termination = apr_palloc(pool,sizeof(mpf_termination_t));
	termination->pool = pool;
	termination->obj = obj;
	termination->event_handler_obj = NULL;
	termination->event_handler = NULL;
	termination->codec_manager = NULL;
	termination->vtable = vtable;
	termination->slot = 0;
	if(audio_stream) {
		audio_stream->termination = termination;
	}
	if(video_stream) {
		video_stream->termination = termination;
	}
	termination->audio_stream = audio_stream;
	termination->video_stream = video_stream;
	return termination;
}

MPF_DECLARE(apt_bool_t) mpf_termination_destroy(mpf_termination_t *termination)
{
	if(termination->vtable && termination->vtable->destroy) {
		termination->vtable->destroy(termination);
	}
	return TRUE;
}

MPF_DECLARE(void*) mpf_termination_object_get(mpf_termination_t *termination)
{
	return termination->obj;
}


apt_bool_t mpf_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	if(termination->vtable && termination->vtable->modify) {
		termination->vtable->modify(termination,descriptor);
	}
	return TRUE;
}




#include "mpf_rtp_stream.h"

static apt_bool_t mpf_rtp_termination_destroy(mpf_termination_t *termination)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	mpf_rtp_termination_descriptor_t *rtp_descriptor = descriptor;
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		audio_stream = mpf_rtp_stream_create(termination,termination->pool);
		if(!audio_stream) {
			return FALSE;
		}
		termination->audio_stream = audio_stream;
	}

	return mpf_rtp_stream_modify(audio_stream,&rtp_descriptor->audio);
}

static const mpf_termination_vtable_t rtp_vtable = {
	mpf_rtp_termination_destroy,
	mpf_rtp_termination_modify,
};

MPF_DECLARE(mpf_termination_t*) mpf_rtp_termination_create(void *obj, apr_pool_t *pool)
{
	return mpf_termination_create(obj,&rtp_vtable,NULL,NULL,pool);
}




#include "mpf_audio_file_stream.h"

static apt_bool_t mpf_file_termination_destroy(mpf_termination_t *termination)
{
	return TRUE;
}

static apt_bool_t mpf_file_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		audio_stream = mpf_file_stream_create(termination,termination->pool);
		if(!audio_stream) {
			return FALSE;
		}
		termination->audio_stream = audio_stream;
	}

	return mpf_file_stream_modify(audio_stream,descriptor);
}

static const mpf_termination_vtable_t file_vtable = {
	mpf_file_termination_destroy,
	mpf_file_termination_modify,
};

MPF_DECLARE(mpf_termination_t*) mpf_file_termination_create(void *obj, apr_pool_t *pool)
{
	return mpf_termination_create(obj,&file_vtable,NULL,NULL,pool);
}
