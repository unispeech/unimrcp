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

static apt_bool_t synth_application_event_handler(const mrcp_application_event_t *app_event);

static apt_bool_t synth_application_on_session_update(demo_application_t *application, mrcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t synth_application_on_session_terminate(demo_application_t *application, mrcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t synth_application_on_channel_modify(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_media_descriptor_t *descriptor)
{
	return TRUE;
}

static apt_bool_t synth_application_on_message_receive(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_application_on_channel_remove(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	return TRUE;
}

static const demo_application_vtable_t synth_application_vtable = {
	synth_application_on_session_update,
	synth_application_on_session_terminate,
	synth_application_on_channel_modify,
	synth_application_on_channel_remove,
	synth_application_on_message_receive
};

demo_application_t* demo_synth_application_create(apr_pool_t *pool)
{
	demo_application_t *synth_application = apr_palloc(pool,sizeof(demo_application_t));
	synth_application->application = NULL;
	synth_application->framework = NULL;
	synth_application->vtable = &synth_application_vtable;
	return synth_application;
}
