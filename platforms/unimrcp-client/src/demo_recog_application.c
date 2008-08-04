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
	/** MRCP signaling channel */
	mrcp_channel_t     *channel;
	/** Audio stream */
	mpf_audio_stream_t *audio_stream;

	apt_bool_t          start_of_input;
};

/** Declaration of recognizer application methods */
static apt_bool_t recog_application_run(demo_application_t *demo_application);
static apt_bool_t recog_application_on_session_update(demo_application_t *demo_application, mrcp_session_t *session);
static apt_bool_t recog_application_on_session_terminate(demo_application_t *demo_application, mrcp_session_t *session);
static apt_bool_t recog_application_on_channel_add(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor);
static apt_bool_t recog_application_on_channel_remove(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel);
static apt_bool_t recog_application_on_message_receive(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const demo_application_vtable_t recog_application_vtable = {
	recog_application_run,
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
	recog_application->name = "recog";
	recog_application->vtable = &recog_application_vtable;
	return recog_application;
}


/** Create demo MRCP message (RECOGNIZE request) */
static mrcp_message_t* recog_application_recognize_message_create(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	const char text[] = 
		"<?xml version=\"1.0\"?>\r\n"
		"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\"\r\n"
		"xml:lang=\"en-US\" version=\"1.0\" root=\"request\">\r\n"
		"<rule id=\"yes\">\r\n"
		"<one-of>\r\n"
		"<item xml:lang=\"fr-CA\">oui</item>\r\n"
		"<item xml:lang=\"en-US\">yes</item>\r\n"
		"</one-of>\r\n"
		"</rule>\r\n"
		"<rule id=\"request\">\r\n"
		"may I speak to\r\n"
		"<one-of xml:lang=\"fr-CA\">\r\n"
		"<item>Michel Tremblay</item>\r\n"
		"<item>Andre Roy</item>\r\n"
		"</one-of>\r\n"
		"</rule>\r\n"
		"</grammar>\r\n";

	/* create MRCP message */
	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header;
		mrcp_generic_header_t *generic_header;
		/* get/allocate generic header */
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			/* set generic header fields */
			apt_string_assign(&generic_header->content_type,"application/synthesis+ssml",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			apt_string_assign(&generic_header->content_id,"request1@form-level.store",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_ID);
		}
		/* get/allocate recognizer header */
		recog_header = mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			/* set recognizer header fields */
			recog_header->cancel_if_queue = FALSE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
		}
		/* set message body */
		apt_string_assign(&mrcp_message->body,text,mrcp_message->pool);
	}
	return mrcp_message;
}

/** Run demo application */
static apt_bool_t recog_application_run(demo_application_t *demo_application)
{
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,NULL);
	if(session) {
		/* create channel */
		mrcp_channel_t *channel;
		mpf_termination_t *termination;
		recog_app_channel_t *recog_channel = apr_palloc(session->pool,sizeof(recog_app_channel_t));
		recog_channel->start_of_input = FALSE;
		recog_channel->audio_stream = mpf_audio_stream_create(recog_channel,&audio_stream_vtable,STREAM_MODE_RECEIVE,session->pool);
		termination = mpf_raw_termination_create(NULL,recog_channel->audio_stream,NULL,session->pool);
		channel = mrcp_application_channel_create(session,MRCP_RECOGNIZER_RESOURCE,termination,recog_channel);
		if(channel) {
			mrcp_message_t *mrcp_message;
			/* add channel to session */
			mrcp_application_channel_add(session,channel,NULL);

			/* create and send RECOGNIZE request */
			mrcp_message = recog_application_recognize_message_create(demo_application,session,channel);
			if(mrcp_message) {
				mrcp_application_message_send(session,channel,mrcp_message);
			}
		}
	}
	return TRUE;
}

static apt_bool_t recog_application_on_session_update(demo_application_t *demo_application, mrcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t recog_application_on_session_terminate(demo_application_t *demo_application, mrcp_session_t *session)
{
	mrcp_application_session_destroy(session);
	return TRUE;
}

static apt_bool_t recog_application_on_channel_add(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor)
{
	return TRUE;
}

static apt_bool_t recog_application_on_channel_remove(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	mrcp_application_session_terminate(session);
	return TRUE;
}

static apt_bool_t recog_application_on_message_receive(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
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
