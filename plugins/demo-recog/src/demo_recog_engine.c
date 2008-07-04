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
#include "mrcp_message.h"

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


/** Create demo recognizer engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) demo_recog_engine_create(apr_pool_t *pool)
{
	mrcp_resource_engine_t *engine = mrcp_resource_engine_create(MRCP_RECOGNIZER_RESOURCE,NULL,&engine_vtable,pool);
	return engine;
}

static apt_bool_t demo_recog_engine_destroy(mrcp_resource_engine_t *engine)
{
	/* destroy recognizer engine */
	return TRUE;
}

static apt_bool_t demo_recog_engine_open(mrcp_resource_engine_t *engine)
{
	/* open recognizer engine */
	return TRUE;
}

static apt_bool_t demo_recog_engine_close(mrcp_resource_engine_t *engine)
{
	/* close recognizer engine */
	return TRUE;
}

static mrcp_engine_channel_t* demo_recog_engine_channel_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	return mrcp_engine_channel_create(engine,&channel_vtable,NULL,NULL,pool);
}

static apt_bool_t demo_recog_channel_destroy(mrcp_engine_channel_t *channel)
{
	return TRUE;
}

static apt_bool_t demo_recog_channel_open(mrcp_engine_channel_t *channel)
{
	/* open channel and send asynch response */
	return mrcp_engine_channel_open_respond(channel,TRUE);
}

static apt_bool_t demo_recog_channel_close(mrcp_engine_channel_t *channel)
{
	/* close channel, make sure there is no activity and send asynch response */
	return mrcp_engine_channel_close_respond(channel);
}

static apt_bool_t demo_recog_channel_define_grammar(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	/* process define-grammar request, and send asynch response */
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_recog_channel_recognize(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	/* process recognize request, and send asynch response */
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_recog_channel_stop(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	/* process stop request, and send asynch response */
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	return mrcp_engine_channel_message_send(channel,response);
}

static apt_bool_t demo_recog_channel_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	apt_bool_t status = FALSE;
	switch(request->start_line.method_id) {
		case RECOGNIZER_SET_PARAMS:
			break;
		case RECOGNIZER_GET_PARAMS:
			break;
		case RECOGNIZER_DEFINE_GRAMMAR:
			break;
		case RECOGNIZER_RECOGNIZE:
			break;
		case RECOGNIZER_GET_RESULT:
			break;
		case RECOGNIZER_START_INPUT_TIMERS:
			break;
		case RECOGNIZER_STOP:
			break;
		default:
			break;
	}
	return status;
}
