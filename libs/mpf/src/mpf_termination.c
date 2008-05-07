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
	termination->vtable = vtable;
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
	mpf_rtp_stream_t *rtp_stream;
	if(!termination->audio_stream) {
		if(rtp_descriptor->audio.mask != RTP_MEDIA_DESCRIPTOR_NONE) {
			termination->audio_stream = mpf_rtp_stream_create(termination->pool);
		}
	}
	rtp_stream = (mpf_rtp_stream_t *)termination->audio_stream;
	if(rtp_stream) {
		mpf_rtp_stream_modify(rtp_stream,&rtp_descriptor->audio);
	}
	return TRUE;
}

static const mpf_termination_vtable_t vtable = {
	mpf_rtp_termination_destroy,
	mpf_rtp_termination_modify,
};

MPF_DECLARE(mpf_termination_t*) mpf_rtp_termination_create(void *obj, apr_pool_t *pool)
{
	return mpf_termination_create(obj,&vtable,NULL,NULL,pool);
}
