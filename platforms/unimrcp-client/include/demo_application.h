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
 * @file demo_application.h
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
	/** MRCP application */
	mrcp_application_t              *application;
	/** Demo framework */
	void                            *framework;
	/** Application name */
	const char                      *name;
	/** Table of demo application virtaul methods */
	const demo_application_vtable_t *vtable;
};

/** Demo application vtable */
struct demo_application_vtable_t {
	/** Virtual run method */
	apt_bool_t (*run)(demo_application_t *application, const char *profile);

	/** Session update event handler */
	apt_bool_t (*on_session_update)(demo_application_t *application, mrcp_session_t *session);
	/** Session terminate event handler */
	apt_bool_t (*on_session_terminate)(demo_application_t *application, mrcp_session_t *session);
	
	/** Channel add event handler */
	apt_bool_t (*on_channel_add)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor);
	/** Channel remove event handler */
	apt_bool_t (*on_channel_remove)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel);

	/** Message receive event handler */
	apt_bool_t (*on_message_receive)(demo_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);
};

/** Create demo synthesizer application */
demo_application_t* demo_synth_application_create(apr_pool_t *pool);

/** Create demo recognizer application */
demo_application_t* demo_recog_application_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__DEMO_APPLICATION_H__*/
