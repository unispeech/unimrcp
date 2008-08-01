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

#include "demo_recog_engine.h"
#include "mrcp_recog_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "apt_consumer_task.h"

typedef struct demo_recog_engine_t demo_recog_engine_t;
typedef struct demo_recog_channel_t demo_recog_channel_t;
typedef struct demo_recog_msg_t demo_recog_msg_t;

/** Declaration of recognizer engine methods */
static apt_bool_t demo_recog_engine_destroy(mrcp_resource_engine_t *engine);
static apt_bool_t demo_recog_engine_open(mrcp_resource_engine_t *engine);
static apt_bool_t demo_recog_engine_close(mrcp_resource_engine_t *engine);
static mrcp_engine_channel_t* demo_recog_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	demo_recog_engine_destroy,
	demo_recog_engine_open,
	demo_recog_engine_close,
	demo_recog_engine_channel_create
};


/** Declaration of recognizer channel methods */
static apt_bool_t demo_recog_channel_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t demo_recog_channel_open(mrcp_engine_channel_t *channel);
static apt_bool_t demo_recog_channel_close(mrcp_engine_channel_t *channel);
static apt_bool_t demo_recog_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	demo_recog_channel_destroy,
	demo_recog_channel_open,
	demo_recog_channel_close,
	demo_recog_channel_request_process
};

/** Declaration of recognizer audio stream methods */
static apt_bool_t demo_recog_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t demo_recog_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t demo_recog_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t demo_recog_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	demo_recog_stream_destroy,
	NULL,
	NULL,
	NULL,
	demo_recog_stream_open,
	demo_recog_stream_close,
	demo_recog_stream_write
};

/** Declaration of demo recognizer engine */
struct demo_recog_engine_t {
	apt_consumer_task_t    *task;
};

/** Declaration of demo recognizer channel */
struct demo_recog_channel_t {
	/** Back pointer to engine */
	demo_recog_engine_t   *demo_engine;
	/** Base engine channel */
	mrcp_engine_channel_t *channel;
	/** Base audio stream */
	mpf_audio_stream_t    *audio_stream;

	/** Active (in-progress) recognition request */
	mrcp_message_t        *recog_request;
	/** Start of recognition input */
	apt_bool_t             start_of_input;
	/** Estimated time to complete */
	apr_size_t             time_to_complete;
};

typedef enum {
	DEMO_RECOG_MSG_OPEN_CHANNEL,
	DEMO_RECOG_MSG_CLOSE_CHANNEL,
	DEMO_RECOG_MSG_REQUEST_PROCESS
} demo_recog_msg_type_e;

/** Declaration of demo recognizer task message */
struct demo_recog_msg_t {
	demo_recog_msg_type_e  type;
	mrcp_engine_channel_t *channel; 
	mrcp_message_t        *request;
};

static apt_bool_t demo_recog_msg_signal(demo_recog_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request);
static apt_bool_t demo_recog_msg_process(apt_task_t *task, apt_task_msg_t *msg);


/** Create demo recognizer engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) demo_recog_engine_create(apr_pool_t *pool)
{
	demo_recog_engine_t *demo_engine = apr_palloc(pool,sizeof(demo_recog_engine_t));
	apt_task_vtable_t task_vtable;
	apt_task_msg_pool_t *msg_pool;

	apt_task_vtable_reset(&task_vtable);
	task_vtable.process_msg = demo_recog_msg_process;
	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(demo_recog_msg_t),pool);
	demo_engine->task = apt_consumer_task_create(demo_engine,&task_vtable,msg_pool,pool);

	return mrcp_resource_engine_create(MRCP_RECOGNIZER_RESOURCE,demo_engine,&engine_vtable,pool);
}

/** Destroy recognizer engine */
static apt_bool_t demo_recog_engine_destroy(mrcp_resource_engine_t *engine)
{
	demo_recog_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_destroy(task);
		demo_engine->task = NULL;
	}
	return TRUE;
}

/** Open recognizer engine */
static apt_bool_t demo_recog_engine_open(mrcp_resource_engine_t *engine)
{
	demo_recog_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_start(task);
	}
	return TRUE;
}

/** Close recognizer engine */
static apt_bool_t demo_recog_engine_close(mrcp_resource_engine_t *engine)
{
	demo_recog_engine_t *demo_engine = engine->obj;
	if(demo_engine->task) {
		apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
		apt_task_terminate(task,TRUE);
	}
	return TRUE;
}

static mrcp_engine_channel_t* demo_recog_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	mpf_termination_t *termination;
	demo_recog_channel_t *recog_channel = apr_palloc(pool,sizeof(demo_recog_channel_t));
	recog_channel->demo_engine = engine->obj;
	recog_channel->recog_request = NULL;
	recog_channel->channel = NULL;
	recog_channel->audio_stream = mpf_audio_stream_create(recog_channel,&audio_stream_vtable,STREAM_MODE_SEND,pool);
	
	termination = mpf_raw_termination_create(NULL,recog_channel->audio_stream,NULL,pool);
	channel = mrcp_engine_channel_create(engine,&channel_vtable,recog_channel,termination,pool);
	recog_channel->channel = channel;
	return channel;
}

static apt_bool_t demo_recog_channel_destroy(mrcp_engine_channel_t *channel)
{
	/* nothing to destroy */
	return TRUE;
}

static apt_bool_t demo_recog_channel_open(mrcp_engine_channel_t *channel)
{
	return demo_recog_msg_signal(DEMO_RECOG_MSG_OPEN_CHANNEL,channel,NULL);
}

static apt_bool_t demo_recog_channel_close(mrcp_engine_channel_t *channel)
{
	return demo_recog_msg_signal(DEMO_RECOG_MSG_CLOSE_CHANNEL,channel,NULL);
}

static apt_bool_t demo_recog_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	return demo_recog_msg_signal(DEMO_RECOG_MSG_REQUEST_PROCESS,channel,request);
}


static apt_bool_t demo_recog_channel_recognize(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process recognize request */
	demo_recog_channel_t *recog_channel = channel->method_obj;
	recog_channel->recog_request = request;
	recog_channel->start_of_input = FALSE;
	recog_channel->time_to_complete = 5000; /* 5 msec */

	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	return TRUE;
}

static apt_bool_t demo_recog_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request, mrcp_message_t *response)
{
	/* process stop request */
	demo_recog_channel_t *recog_channel = channel->method_obj;
	recog_channel->recog_request = NULL;
	return TRUE;
}

static apt_bool_t demo_recog_channel_request_dispatch(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	switch(request->start_line.method_id) {
		case RECOGNIZER_SET_PARAMS:
			break;
		case RECOGNIZER_GET_PARAMS:
			break;
		case RECOGNIZER_DEFINE_GRAMMAR:
			break;
		case RECOGNIZER_RECOGNIZE:
			status = demo_recog_channel_recognize(channel,request,response);
			break;
		case RECOGNIZER_GET_RESULT:
			break;
		case RECOGNIZER_START_INPUT_TIMERS:
			break;
		case RECOGNIZER_STOP:
			status = demo_recog_channel_stop(channel,request,response);
			break;
		default:
			break;
	}
	/* send asynchronous response */
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_recog_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_recog_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_recog_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t demo_recog_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	demo_recog_channel_t *recog_channel = stream->obj;
	if(recog_channel->recog_request) {
		if((frame->type & MEDIA_FRAME_TYPE_AUDIO) == MEDIA_FRAME_TYPE_AUDIO) {
			/* process audio stream */
			
			if(recog_channel->start_of_input == FALSE) {
				/* raise START-OF-INPUT event */
				mrcp_message_t *message = mrcp_event_create(
									recog_channel->recog_request,
									RECOGNIZER_START_OF_INPUT,
									recog_channel->recog_request->pool);
				if(message) {
					mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(message);
					if(recog_header) {
						recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_SUCCESS;
						mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
					}
					message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
					mrcp_engine_channel_message_send(recog_channel->channel,message);
				}
				recog_channel->start_of_input = TRUE;
			}
		}

		if(recog_channel->start_of_input == TRUE) {
			if(recog_channel->time_to_complete >= CODEC_FRAME_TIME_BASE) {
				recog_channel->time_to_complete -= CODEC_FRAME_TIME_BASE;
			}
			else {
				/* raise RECOGNITION-COMPLETE event */
				mrcp_message_t *message = mrcp_event_create(
									recog_channel->recog_request,
									RECOGNIZER_RECOGNITION_COMPLETE,
									recog_channel->recog_request->pool);
				if(message) {
					mrcp_recog_header_t *recog_header = mrcp_resource_header_prepare(message);
					if(recog_header) {
						recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_SUCCESS;
						mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
					}
					message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

					recog_channel->recog_request = NULL;
					mrcp_engine_channel_message_send(recog_channel->channel,message);
				}
			}
		}
	}
	return TRUE;
}

static apt_bool_t demo_recog_msg_signal(demo_recog_msg_type_e type, mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	demo_recog_channel_t *demo_channel = channel->method_obj;
	demo_recog_engine_t *demo_engine = demo_channel->demo_engine;
	apt_task_t *task = apt_consumer_task_base_get(demo_engine->task);
	apt_task_msg_t *msg = apt_task_msg_get(task);
	if(msg) {
		demo_recog_msg_t *demo_msg;
		msg->type = TASK_MSG_USER;
		demo_msg = (demo_recog_msg_t*) msg->data;

		demo_msg->type = type;
		demo_msg->channel = channel;
		demo_msg->request = request;
		status = apt_task_msg_signal(task,msg);
	}
	return status;
}

static apt_bool_t demo_recog_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	demo_recog_msg_t *demo_msg = (demo_recog_msg_t*)msg->data;
	switch(demo_msg->type) {
		case DEMO_RECOG_MSG_OPEN_CHANNEL:
			/* open channel and send asynch response */
			mrcp_engine_channel_open_respond(demo_msg->channel,TRUE);
			break;
		case DEMO_RECOG_MSG_CLOSE_CHANNEL:
			/* close channel, make sure there is no activity and send asynch response */
			mrcp_engine_channel_close_respond(demo_msg->channel);
			break;
		case DEMO_RECOG_MSG_REQUEST_PROCESS:
			demo_recog_channel_request_dispatch(demo_msg->channel,demo_msg->request);
			break;
		default:
			break;
	}
	return TRUE;
}
