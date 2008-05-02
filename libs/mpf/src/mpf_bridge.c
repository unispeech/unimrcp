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

#include "mpf_bridge.h"
#include "mpf_frame.h"
#include "apt_log.h"

static apt_bool_t mpf_bridge_process(mpf_object_t *object)
{
	object->frame.type = MEDIA_FRAME_TYPE_NONE;
	object->src_stream->vtable->read_frame(object->src_stream,&object->frame);
	
	if((object->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		memset(	object->frame.codec_frame.buffer,
				0,
				object->frame.codec_frame.size);
	}

	object->dest_stream->vtable->write_frame(object->dest_stream,&object->frame);
	return TRUE;
}

static apt_bool_t mpf_bridge_destroy(mpf_object_t *object)
{
	apt_log(APT_PRIO_DEBUG,"Destroy Audio Bridge");
	return TRUE;
}

MPF_DECLARE(mpf_object_t*) mpf_bridge_create(mpf_audio_stream_t *src_stream, mpf_audio_stream_t *dest_stream, apr_pool_t *pool)
{
	mpf_object_t *bridge;
	apr_size_t frame_size;
	if(!src_stream || !dest_stream) {
		return NULL;
	}
	apt_log(APT_PRIO_DEBUG,"Create Audio Bridge");
	bridge = apr_palloc(pool,sizeof(mpf_object_t));
	bridge->src_stream = src_stream;
	bridge->dest_stream = dest_stream;
	bridge->process = mpf_bridge_process;
	bridge->destroy = mpf_bridge_destroy;

	frame_size = 2 * CODEC_FRAME_TIME_BASE * /*sampling_rate*/8000 / 1000;
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return bridge;
}
