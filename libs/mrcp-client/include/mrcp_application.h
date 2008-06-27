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

#include "mrcp_client_types.h"
#include "mpf_rtp_descriptor.h"

APT_BEGIN_EXTERN_C

/** MRCP application message declaration */
typedef struct mrcp_app_message_t mrcp_app_message_t;

/** MRCP application event handler declaration */
typedef apt_bool_t (*mrcp_app_message_handler_f)(const mrcp_app_message_t *app_message);

/** Enumeration of MRCP application message types */
typedef enum {
	MRCP_APP_MESSAGE_TYPE_REQUEST,  /**< request message */
	MRCP_APP_MESSAGE_TYPE_RESPONSE, /**< response message */
	MRCP_APP_MESSAGE_TYPE_EVENT     /**< event message */
} mrcp_app_message_type_e;

/** Enumeration of MRCP application status codes */
typedef enum {
	MRCP_APP_STATUS_CODE_SUCCESS,   /**< indicates success */
	MRCP_APP_STATUS_CODE_FAILURE,   /**< indicates failure */
	MRCP_APP_STATUS_CODE_TERMINATE
} mrcp_app_status_code_e;


/** Enumeration of MRCP application commands (requests/responses) */
typedef enum {
	MRCP_APP_COMMAND_SESSION_UPDATE,
	MRCP_APP_COMMAND_SESSION_TERMINATE,
	MRCP_APP_COMMAND_CHANNEL_ADD,
	MRCP_APP_COMMAND_CHANNEL_REMOVE,
	MRCP_APP_COMMAND_MESSAGE,
} mrcp_app_command_e;

/** Enumeration of MRCP application events */
typedef enum {
	MRCP_APP_EVENT_MESSAGE,
} mrcp_app_event_e;


/** MRCP application message definition */
struct mrcp_app_message_t {
	/** Message type (request/response/event) */
	mrcp_app_message_type_e           message_type;
	/** Command (request/response) identifier */
	mrcp_app_command_e                command_id;
	/** Event identifier */
	mrcp_app_event_e                  event_id;
	/** Status code used in response */
	mrcp_app_status_code_e            status;

	/** Application */
	mrcp_application_t               *application;
	/** Session */
	mrcp_session_t                   *session;
	/** Channel */
	mrcp_channel_t                   *channel;
	/** MRCP message */
	mrcp_message_t                   *mrcp_message;
	/** Optional RTP descriptor */
	mpf_rtp_termination_descriptor_t *descriptor;
};

/**
 * Create application instance.
 * @param obj the external object
 * @param vtable the event handlers
 * @param pool the memory pool to allocate memory from
 */
MRCP_DECLARE(mrcp_application_t*) mrcp_application_create(void *obj, mrcp_version_e version, const mrcp_app_message_handler_f handler, apr_pool_t *pool);

/**
 * Destroy application instance.
 * @param application the application to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_destroy(mrcp_application_t *application);

/**
 * Get external object associated with the application.
 * @param application the application to get object from
 */
APT_DECLARE(void*) mrcp_application_object_get(mrcp_application_t *application);

/**
 * Create session.
 * @param application the entire application
 * @param obj the external object
 * @return the created session instance
 */
MRCP_DECLARE(mrcp_session_t*) mrcp_application_session_create(mrcp_application_t *application, void *obj);

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
 * Send channel add request.
 * @param session the session to create channel for
 * @param channel the control channel
 * @param descriptor the descriptor of RTP termination assoiciated with control channel (NULL by default)
 */
MRCP_DECLARE(apt_bool_t) mrcp_application_channel_add(mrcp_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *descriptor);

/** 
 * Create MRCP message.
 * @param session the session
 * @param channel the control channel
 * @param method_id the method identifier of MRCP message
 */
mrcp_message_t* mrcp_application_message_create(mrcp_session_t *session, mrcp_channel_t *channel, mrcp_method_id method_id);

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
