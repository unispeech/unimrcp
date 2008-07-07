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
#include "mrcp_recog_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_generic_header.h"

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

	mrcp_message_t *mrcp_message = mrcp_application_message_create(session,channel,RECOGNIZER_RECOGNIZE);
	if(mrcp_message) {
		mrcp_recog_header_t *recog_header;
		mrcp_generic_header_t *generic_header;
		generic_header = mrcp_generic_header_prepare(mrcp_message);
		if(generic_header) {
			apt_string_assign(&generic_header->content_type,"application/synthesis+ssml",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_TYPE);
			apt_string_assign(&generic_header->content_id,"request1@form-level.store",mrcp_message->pool);
			mrcp_generic_header_property_add(mrcp_message,GENERIC_HEADER_CONTENT_ID);
		}
		recog_header = mrcp_resource_header_prepare(mrcp_message);
		if(recog_header) {
			recog_header->cancel_if_queue = FALSE;
			mrcp_resource_header_property_add(mrcp_message,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
		}
		apt_string_assign(&mrcp_message->body,text,mrcp_message->pool);
	}
	return mrcp_message;
}


static apt_bool_t recog_application_run(demo_application_t *demo_application)
{
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,NULL);
	if(session) {
		mrcp_channel_t *channel = mrcp_application_channel_create(session,MRCP_SYNTHESIZER_RESOURCE,NULL,NULL);
		if(channel) {
			mrcp_message_t *mrcp_message;
			mrcp_application_channel_add(session,channel,NULL);

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

static apt_bool_t recog_application_on_message_receive(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	mrcp_application_channel_remove(session,channel);
	return TRUE;
}

static apt_bool_t recog_application_on_channel_remove(demo_application_t *demo_application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	mrcp_application_session_terminate(session);
	return TRUE;
}

static const demo_application_vtable_t recog_application_vtable = {
	recog_application_run,
	recog_application_on_session_update,
	recog_application_on_session_terminate,
	recog_application_on_channel_add,
	recog_application_on_channel_remove,
	recog_application_on_message_receive
};

demo_application_t* demo_recog_application_create(apr_pool_t *pool)
{
	demo_application_t *recog_application = apr_palloc(pool,sizeof(demo_application_t));
	recog_application->application = NULL;
	recog_application->framework = NULL;
	recog_application->vtable = &recog_application_vtable;
	return recog_application;
}
