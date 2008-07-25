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

#include "demo_synth_engine.h"
#include "mrcp_synth_resource.h"
#include "mrcp_message.h"

/** Declaration of synthesizer engine methods */
static apt_bool_t demo_synth_engine_destroy(mrcp_resource_engine_t *engine);
static apt_bool_t demo_synth_engine_open(mrcp_resource_engine_t *engine);
static apt_bool_t demo_synth_engine_close(mrcp_resource_engine_t *engine);
static mrcp_engine_channel_t* demo_synth_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	demo_synth_engine_destroy,
	demo_synth_engine_open,
	demo_synth_engine_close,
	demo_synth_engine_channel_create
};


/** Declaration of synthesizer channel methods */
static apt_bool_t demo_synth_channel_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t demo_synth_channel_open(mrcp_engine_channel_t *channel);
static apt_bool_t demo_synth_channel_close(mrcp_engine_channel_t *channel);
static apt_bool_t demo_synth_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	demo_synth_channel_destroy,
	demo_synth_channel_open,
	demo_synth_channel_close,
	demo_synth_channel_request_process
};

/** Declaration of synthesizer audio stream */
typedef struct demo_synth_stream_t demo_synth_stream_t;
struct demo_synth_stream_t {
	mpf_audio_stream_t base;
};

/** Declaration of synthesizer stream methods */
static apt_bool_t demo_synth_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t demo_synth_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t demo_synth_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t demo_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	demo_synth_stream_destroy,
	demo_synth_stream_open,
	demo_synth_stream_close,
	demo_synth_stream_read,
	NULL,
	NULL,
	NULL
};


/** Create demo synthesizer engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) demo_synth_engine_create(apr_pool_t *pool)
{
	mrcp_resource_engine_t *engine = mrcp_resource_engine_create(MRCP_SYNTHESIZER_RESOURCE,NULL,&engine_vtable,pool);
	return engine;
}

static apt_bool_t demo_synth_engine_destroy(mrcp_resource_engine_t *engine)
{
	/* destroy synthesizer engine */
	return TRUE;
}

static apt_bool_t demo_synth_engine_open(mrcp_resource_engine_t *engine)
{
	/* open synthesizer engine */
	return TRUE;
}

static apt_bool_t demo_synth_engine_close(mrcp_resource_engine_t *engine)
{
	/* close synthesizer engine */
	return TRUE;
}

static mrcp_engine_channel_t* demo_synth_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	mpf_termination_t *termination;
	demo_synth_stream_t *synth_stream = apr_palloc(pool,sizeof(demo_synth_stream_t));
	mpf_audio_stream_init(&synth_stream->base,&audio_stream_vtable);
	termination = mpf_raw_termination_create(NULL,&synth_stream->base,NULL,pool);
	return mrcp_engine_channel_create(engine,&channel_vtable,NULL,termination,pool);
}

static apt_bool_t demo_synth_channel_destroy(mrcp_engine_channel_t *channel)
{
	return TRUE;
}

static apt_bool_t demo_synth_channel_open(mrcp_engine_channel_t *channel)
{
	/* open channel and send asynch response */
	return mrcp_engine_channel_open_respond(channel,TRUE);
}

static apt_bool_t demo_synth_channel_close(mrcp_engine_channel_t *channel)
{
	/* close channel, make sure there is no activity and send asynch response */
	return mrcp_engine_channel_close_respond(channel);
}

static apt_bool_t demo_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	/* process speak request, and send asynch response */
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	/* process stop request, and send asynch response */
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_synth_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	switch(request->start_line.method_id) {
		case SYNTHESIZER_SET_PARAMS:
			break;
		case SYNTHESIZER_GET_PARAMS:
			break;
		case SYNTHESIZER_SPEAK:
			status = demo_synth_channel_speak(channel,request);
			break;
		case SYNTHESIZER_STOP:
			status = demo_synth_channel_stop(channel,request);
			break;
		case SYNTHESIZER_PAUSE:
			break;
		case SYNTHESIZER_RESUME:
			break;
		case SYNTHESIZER_BARGE_IN_OCCURRED:
			break;
		case SYNTHESIZER_CONTROL:
			break;
		case SYNTHESIZER_DEFINE_LEXICON:
			break;
		default:
			break;
	}
	return status;
}


static apt_bool_t demo_synth_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_synth_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_synth_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_synth_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	return TRUE;
}
