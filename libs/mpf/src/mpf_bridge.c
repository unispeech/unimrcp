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
	object->source->vtable->read_frame(object->source,&object->frame);
	
	if((object->frame.type & MEDIA_FRAME_TYPE_AUDIO) == 0) {
		memset(	object->frame.codec_frame.buffer,
				0,
				object->frame.codec_frame.size);
	}

	object->sink->vtable->write_frame(object->sink,&object->frame);
	return TRUE;
}

static apt_bool_t mpf_bridge_destroy(mpf_object_t *object)
{
	apt_log(APT_PRIO_DEBUG,"Destroy Audio Bridge");
	return TRUE;
}

MPF_DECLARE(mpf_object_t*) mpf_bridge_create(mpf_audio_stream_t *source, mpf_audio_stream_t *sink, apr_pool_t *pool)
{
	mpf_object_t *bridge;
	apr_size_t frame_size;
	if(!source || !sink) {
		return NULL;
	}
	apt_log(APT_PRIO_DEBUG,"Create Audio Bridge");
	bridge = apr_palloc(pool,sizeof(mpf_object_t));
	bridge->source = source;
	bridge->sink = sink;
	bridge->process = mpf_bridge_process;
	bridge->destroy = mpf_bridge_destroy;

	frame_size = mpf_codec_linear_frame_size_calculate(8000,1);
	bridge->frame.codec_frame.size = frame_size;
	bridge->frame.codec_frame.buffer = apr_palloc(pool,frame_size);
	return bridge;
}
