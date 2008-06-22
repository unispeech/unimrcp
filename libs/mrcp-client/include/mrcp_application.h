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

#ifndef __MRCP_APPLICATION_H__
#define __MRCP_APPLICATION_H__

/**
 * @file mrcp_application.h
 * @brief MRCP User Level Application Interface
 */ 

#include "mrcp_sig_types.h"
#include "mpf_types.h"
#include "mpf_rtp_descriptor.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP application declaration */
typedef struct mrcp_application_t mrcp_application_t;
/** Opaque MRCP channel declaration */
typedef struct mrcp_channel_t mrcp_channel_t;
/** MRCP application event declaration */
typedef struct mrcp_application_event_t mrcp_application_event_t;

/** MRCP application event handler declaration */
typedef apt_bool_t (*mrcp_application_event_handler_f)(const mrcp_application_event_t *app_event);

typedef enum {
	MRCP_APPLICATION_EVENT_SESSION_UPDATE,
	MRCP_APPLICATION_EVENT_SESSION_TERMINATE,
	MRCP_APPLICATION_EVENT_CHANNEL_MODIFY,
	MRCP_APPLICATION_EVENT_CHANNEL_REMOVE,
	MRCP_APPLICATION_EVENT_MESSAGE_RECEIVE,
} mrcp_application_event_type_e;

struct mrcp_application_event_t {
	mrcp_application_event_type_e type;
	mrcp_application_t           *application;
	mrcp_session_t               *session;
	mrcp_channel_t               *channel;
	mrcp_message_t               *message;
	mpf_rtp_media_descriptor_t   *descriptor;
};

/**
 * Create application instance.
 * @param obj the external object
 * @param vtable the event handlers
 * @param pool the memory pool to allocate memory from
 */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(void *obj, mrcp_version_e version, const mrcp_application_event_handler_f event_handler, apr_pool_t *pool);

/**
 * Destroy application instance.
 * @param application the application to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_destroy(mrcp_application_t *application);


/**
 * Create client session.
 * @param application the entire application
 * @param obj the external object
 * @return the created session instance
 */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, void *obj);

/**
 * Get external object associated with the application.
 * @param application the application to get object from
 */
APT_DECLARE(void*) mrcp_application_object_get(mrcp_application_t *application);

/** 
 * Send session update request.
 * @param session the session to update
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_update(mrcp_session_t *session);

/** 
 * Send session termination request.
 * @param session the session to terminate
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_terminate(mrcp_session_t *session);

/** 
 * Destroy client session (session must be terminated prior to destroy).
 * @param session the session to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_session_destroy(mrcp_session_t *session);


/** 
 * Create control channel.
 * @param session the session to create channel for
 * @param termination the media termination
 * @param obj the external object
 */
MRCP_DECLARE(mrcp_channel_t*) mrcp_application_channel_create(mrcp_session_t *session, mrcp_resource_id resource_id, mpf_termination_t *termination, void *obj);

/** 
 * Send channel add/modify request.
 * @param session the session to create channel for
 * @param channel the control channel
 * @param descriptor the descriptor of RTP termination assoiciated with control channel (NULL by default)
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_modify(mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor);

/** 
 * Send MRCP message.
 * @param session the session
 * @param channel the control channel
 * @param message the MRCP message to send
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_message_send(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

/** 
 * Remove channel.
 * @param session the session to remove channel from
 * @param channel the control channel to remove
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_remove(mrcp_session_t *session, mrcp_channel_t *channel);

/** 
 * Destroy channel.
 * @param channel the control channel to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_destroy(mrcp_channel_t *channel);


APT_END_EXTERN_C

#endif /*__MRCP_APPLICATION_H__*/
