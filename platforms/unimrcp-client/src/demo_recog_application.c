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

#include "demo_application.h"
#include "demo_util.h"
#include "mrcp_session.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "mpf_termination.h"
#include "mpf_stream.h"

typedef struct recog_app_channel_t recog_app_channel_t;

/** Declaration of recognizer application channel */
struct recog_app_channel_t {
	/** MRCP control channel */
	mrcp_channel_t     *channel;
	/** Audio stream */
	mpf_audio_stream_t *audio_stream;

	apt_bool_t          start_of_input;
};

/** Declaration of recognizer application methods */
static apt_bool_t recog_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t recog_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

/** Declaration of application message handlers */
static apt_bool_t recog_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const mrcp_app_message_dispatcher_t recog_application_dispatcher = {
	recog_application_on_session_update,
	recog_application_on_session_terminate,
	recog_application_on_channel_add,
	recog_application_on_channel_remove,
	recog_application_on_message_receive
};

/** Declaration of recognizer audio stream methods */
static apt_bool_t recog_app_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	recog_app_stream_destroy,
	recog_app_stream_open,
	recog_app_stream_close,
	recog_app_stream_read,
	NULL,
	NULL,
	NULL
};


/** Create demo recognizer application */
demo_application_t* demo_recog_application_create(apr_pool_t *pool)
{
	demo_application_t *recog_application = apr_palloc(pool,sizeof(demo_application_t));
	recog_application->application = NULL;
	recog_application->framework = NULL;
	recog_application->handler = recog_application_handler;
	recog_application->run = recog_application_run;
	return recog_application;
}

/** Run demo application */
static apt_bool_t recog_application_run(demo_application_t *demo_application, const char *profile)
{
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(session) {
		/* create channel */
		mrcp_channel_t *channel;
		mpf_termination_t *termination;
		recog_app_channel_t *recog_channel = apr_palloc(session->pool,sizeof(recog_app_channel_t));
		recog_channel->start_of_input = FALSE;
		recog_channel->audio_stream = mpf_audio_stream_create(recog_channel,&audio_stream_vtable,STREAM_MODE_RECEIVE,session->pool);
		termination = mpf_raw_termination_create(NULL,recog_channel->audio_stream,NULL,session->pool);
		channel = mrcp_application_channel_create(session,MRCP_RECOGNIZER_RESOURCE,termination,NULL,recog_channel);
		if(channel) {
			mrcp_message_t *mrcp_message;
			/* add channel to session */
			mrcp_application_channel_add(session,channel);

			/* create and send RECOGNIZE request */
			mrcp_message = demo_recognize_message_create(session,channel);
			if(mrcp_message) {
				mrcp_application_message_send(session,channel,mrcp_message);
			}
		}
	}
	return TRUE;
}

static apt_bool_t recog_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	return mrcp_application_message_dispatch(&recog_application_dispatcher,app_message);
}

static apt_bool_t recog_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	return TRUE;
}

static apt_bool_t recog_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	mrcp_application_session_destroy(session);
	return TRUE;
}

static apt_bool_t recog_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	return TRUE;
}

static apt_bool_t recog_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	mrcp_application_session_terminate(session);
	return TRUE;
}

static apt_bool_t recog_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	recog_app_channel_t *recog_channel = mrcp_application_channel_object_get(channel);
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received response to RECOGNIZE request, start to stream the speech to recognize */
		if(recog_channel) {
			recog_channel->start_of_input = TRUE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		if(message->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) {
			if(recog_channel) {
				recog_channel->start_of_input = FALSE;
			}
			mrcp_application_channel_remove(session,channel);
		}
	}
	return TRUE;
}

static apt_bool_t recog_app_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t recog_app_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t recog_app_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t recog_app_stream_read(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	recog_app_channel_t *recog_channel = stream->obj;
	if(recog_channel && recog_channel->start_of_input == TRUE) {
		frame->type |= MEDIA_FRAME_TYPE_AUDIO;
		memset(frame->codec_frame.buffer,0,frame->codec_frame.size);
	}
	return TRUE;
}
