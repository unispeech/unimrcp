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
#include "mrcp_synth_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "apt_consumer_task.h"

typedef struct demo_synth_engine_t demo_synth_engine_t;
typedef struct demo_synth_channel_t demo_synth_channel_t;
typedef struct demo_synth_msg_t demo_synth_msg_t;

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

/** Declaration of synthesizer audio stream methods */
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

/** Declaration of demo synthesizer engine */
struct demo_synth_engine_t {
	apt_consumer_task_t    *task;
};

/** Declaration of demo synthesizer channel */
struct demo_synth_channel_t {
	/** Back pointer to engine */
	demo_synth_engine_t   *demo_engine;
	/** Base engine channel */
	mrcp_engine_channel_t *channel;
	/** Base audio stream */
	mpf_audio_stream_t     *audio_stream;

	/** Active (in-progress) speak request */
	mrcp_message_t        *speak_request;
	/** Estimated time to complete */
	apr_size_t             time_to_complete;
};

typedef enum {
	DEMO_SYNTH_MSG_OPEN_CHANNEL,
	DEMO_SYNTH_MSG_CLOSE_CHANNEL,
	DEMO_SYNTH_MSG_REQUEST_PROCESS
} demo_synth_msg_type_e;

/** Declaration of demo synthesizer task message */
struct demo_synth_msg_t {
	demo_synth_msg_type_e  type;
	mrcp_engine_channel_t *channel; 
	mrcp_message_t        *request;
};

static apt_bool_t demo_synth_msg_signal(demo_synth_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request);
static apt_bool_t demo_synth_msg_process(apt_task_t *task, apt_task_msg_t *msg);


/** Create demo synthesizer engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) demo_synth_engine_create(apr_pool_t *pool)
{
	demo_synth_engine_t *demo_engine = apr_palloc(pool,sizeof(demo_synth_engine_t));
	apt_task_vtable_t task_vtable;
	apt_task_msg_pool_t *msg_pool;

	apt_task_vtable_reset(&task_vtable);
	task_vtable.process_msg = demo_synth_msg_process;
	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(demo_synth_msg_t),pool);
	demo_engine->task = apt_consumer_task_create(demo_engine,&task_vtable,msg_pool,pool);

	return mrcp_resource_engine_create(MRCP_SYNTHESIZER_RESOURCE,demo_engine,&engine_vtable,pool);
}

/** Destroy synthesizer engine */
static apt_bool_t demo_synth_engine_destroy(mrcp_resource_engine_t *engine)
{
	demo_synth_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_destroy(task);
		demo_engine->task = NULL;
	}
	return TRUE;
}

/** Open synthesizer engine */
static apt_bool_t demo_synth_engine_open(mrcp_resource_engine_t *engine)
{
	demo_synth_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_start(task);
	}
	return TRUE;
}

/** Close synthesizer engine */
static apt_bool_t demo_synth_engine_close(mrcp_resource_engine_t *engine)
{
	demo_synth_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_terminate(task,TRUE);
	}
	return TRUE;
}

static mrcp_engine_channel_t* demo_synth_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	mpf_termination_t *termination;
	demo_synth_channel_t *synth_channel = apr_palloc(pool,sizeof(demo_synth_channel_t));
	synth_channel->demo_engine = engine->obj;
	synth_channel->speak_request = NULL;
	synth_channel->channel = NULL;
	synth_channel->audio_stream = mpf_audio_stream_create(synth_channel,&audio_stream_vtable,STREAM_MODE_RECEIVE,pool);
	
	termination = mpf_raw_termination_create(NULL,synth_channel->audio_stream,NULL,pool);
	channel = mrcp_engine_channel_create(engine,&channel_vtable,synth_channel,termination,pool);
	synth_channel->channel = channel;
	return channel;
}

static apt_bool_t demo_synth_channel_destroy(mrcp_engine_channel_t *channel)
{
	/* nothing to destroy */
	return TRUE;
}

static apt_bool_t demo_synth_channel_open(mrcp_engine_channel_t *channel)
{
	return demo_synth_msg_signal(DEMO_SYNTH_MSG_OPEN_CHANNEL,channel,NULL);
}

static apt_bool_t demo_synth_channel_close(mrcp_engine_channel_t *channel)
{
	return demo_synth_msg_signal(DEMO_SYNTH_MSG_CLOSE_CHANNEL,channel,NULL);
}

static apt_bool_t demo_synth_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	return demo_synth_msg_signal(DEMO_SYNTH_MSG_REQUEST_PROCESS,channel,request);
}

static apt_bool_t demo_synth_channel_speak(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process speak request */
	demo_synth_channel_t *synth_channel = channel->method_obj;
	synth_channel->speak_request = request;
	synth_channel->time_to_complete = 0;
	if(mrcp_generic_header_property_check(request,GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(request);
		if(generic_header) {
			synth_channel->time_to_complete = generic_header->content_length * 10; /* 10 msec per character */
		}
	}
	
	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return TRUE;
}

static apt_bool_t demo_synth_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process stop request */
	demo_synth_channel_t *synth_channel = channel->method_obj;
	synth_channel->speak_request = NULL;
	return TRUE;
}

static apt_bool_t demo_synth_channel_request_dispatch(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	switch(request->start_line.method_id) {
		case SYNTHESIZER_SET_PARAMS:
			break;
		case SYNTHESIZER_GET_PARAMS:
			break;
		case SYNTHESIZER_SPEAK:
			status = demo_synth_channel_speak(channel,request,response);
			break;
		case SYNTHESIZER_STOP:
			status = demo_synth_channel_stop(channel,request,response);
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
	/* send asynchronous response */
	return mrcp_engine_channel_message_send(channel,response);
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
	demo_synth_channel_t *synth_channel = stream->obj;
	if(synth_channel->speak_request) {
		frame->type |= MEDIA_FRAME_TYPE_AUDIO;
		memset(frame->codec_frame.buffer,0,frame->codec_frame.size);

		if(synth_channel->time_to_complete >= CODEC_FRAME_TIME_BASE) {
			synth_channel->time_to_complete -= CODEC_FRAME_TIME_BASE;
		}
		else {
			/* raise SPEAK-COMPLETE event */
			mrcp_message_t *message = mrcp_event_create(
								synth_channel->speak_request,
								SYNTHESIZER_SPEAK_COMPLETE,
								synth_channel->speak_request->pool);
			if(message) {
				mrcp_synth_header_t *synth_header = mrcp_resource_header_prepare(message);
				if(synth_header) {
					synth_header->completion_cause = SYNTHESIZER_COMPLETION_CAUSE_NORMAL;
					mrcp_resource_header_property_add(message,SYNTHESIZER_HEADER_COMPLETION_CAUSE);
				}
				message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

				synth_channel->speak_request = NULL;
				mrcp_engine_channel_message_send(synth_channel->channel,message);
			}
		}
	}
	return TRUE;
}

static apt_bool_t demo_synth_msg_signal(demo_synth_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	demo_synth_channel_t *demo_channel = channel->method_obj;
	demo_synth_engine_t *demo_engine = demo_channel->demo_engine;
	apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
	apt_task_msg_t *msg = apt_task_msg_get(task);
	if(msg) {
		demo_synth_msg_t *demo_msg;
		msg->type = TASK_MSG_USER;
		demo_msg = (demo_synth_msg_t*) msg->data;

		demo_msg->type = type;
		demo_msg->channel = channel;
		demo_msg->request = request;
		status = apt_task_msg_signal(task,msg);
	}
	return status;
}

static apt_bool_t demo_synth_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	demo_synth_msg_t *demo_msg = (demo_synth_msg_t*)msg->data;
	switch(demo_msg->type) {
		case DEMO_SYNTH_MSG_OPEN_CHANNEL:
			/* open channel and send asynch response */
			mrcp_engine_channel_open_respond(demo_msg->channel,TRUE);
			break;
		case DEMO_SYNTH_MSG_CLOSE_CHANNEL:
			/* close channel, make sure there is no activity and send asynch response */
			mrcp_engine_channel_close_respond(demo_msg->channel);
			break;
		case DEMO_SYNTH_MSG_REQUEST_PROCESS:
			demo_synth_channel_request_dispatch(demo_msg->channel,demo_msg->request);
			break;
		default:
			break;
	}
	return TRUE;
}
