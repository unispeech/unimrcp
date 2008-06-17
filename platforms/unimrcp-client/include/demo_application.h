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

#ifndef __DEMO_APPLICATION_H__
#define __DEMO_APPLICATION_H__

/**
 * @file demo_framework.h
 * @brief Demo MRCP Application Framework
 */ 

#include "mrcp_application.h"

APT_BEGIN_EXTERN_C

/** Demo application declaration */
typedef struct demo_application_t demo_application_t;
/** Demo application vtable declaration */
typedef struct demo_application_vtable_t demo_application_vtable_t;

/** Demo application */
struct demo_application_t {
	mrcp_application_t              *application;
	void                            *framework;
	const demo_application_vtable_t *vtable;
};

/** Demo application vtable */
struct demo_application_vtable_t {
	apt_bool_t (*run)(demo_application_t *application);

	apt_bool_t (*on_session_update)(demo_application_t *application, mrcp_session_t *session);
	apt_bool_t (*on_session_terminate)(demo_application_t *application, mrcp_session_t *session);
	
	apt_bool_t (*on_channel_modify)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_media_descriptor_t *descriptor);
	apt_bool_t (*on_channel_remove)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel);

	apt_bool_t (*on_message_receive)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);
};


demo_application_t* demo_synth_application_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__DEMO_APPLICATION_H__*/
