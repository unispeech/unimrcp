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

#ifndef __MRCP_CLIENT_SESSION_H__
#define __MRCP_CLIENT_SESSION_H__

/**
 * @file mrcp_client_session.h
 * @brief MRCP Client Session
 */ 

#include "mrcp_client_types.h"
#include "mrcp_application.h"
#include "mrcp_session.h"
#include "mpf_message.h"
#include "apt_task_msg.h"
#include "apt_obj_list.h"

APT_BEGIN_EXTERN_C

/** MRCP client session declaration */
typedef struct mrcp_client_session_t mrcp_client_session_t;

/** MRCP client session */
struct mrcp_client_session_t {
	/** Session base */
	mrcp_session_t             base;
	/** Application session belongs to */
	mrcp_application_t        *application;

	/** Media context */
	mpf_context_t             *context;

	/** RTP termination array (mrcp_termination_slot_t) */
	apr_array_header_t        *terminations;
	/** MRCP control channel array (mrcp_channel_t*) */
	apr_array_header_t        *channels;

	/** In-progress offer */
	mrcp_session_descriptor_t *offer;
	/** In-progress answer */
	mrcp_session_descriptor_t *answer;

	/** MRCP application active request */
	const mrcp_app_message_t  *active_request;
	/** MRCP application request queue */
	apt_obj_list_t            *request_queue;

	/** Number of in-progress offer requests (flags) */
	apr_size_t                 offer_flag_count;
	/** Number of in-progress answer requests (flags) */
	apr_size_t                 answer_flag_count;
	/** Number of in-progress terminate requests (flags) */
	apr_size_t                 terminate_flag_count;
};

/** MRCP channel */
struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t             *pool;
	/** External object associated with channel */
	void                   *obj;
	/** MRCP resource identifier */
	mrcp_resource_id        resource_id;
	/** MRCP resource */
	mrcp_resource_t        *resource;
	/** MRCP session entire channel belongs to */
	mrcp_session_t         *session;
	/** MRCP control channel */
	mrcp_control_channel_t *control_channel;
	/** Media termination */
	mpf_termination_t      *termination;

	/** waiting state of control media */
	apt_bool_t              waiting_for_channel;
	/** waiting state of media termination */
	apt_bool_t              waiting_for_termination;
};

/** MRCP application */
struct mrcp_application_t {
	/** External object associated with the application */
	void                      *obj;
	/** MRCP protocol version */
	mrcp_version_e             version;
	/** Application message handler */
	mrcp_app_message_handler_f handler;
	/** MRCP client */
	mrcp_client_t             *client;
	/** Application task message pool */
	apt_task_msg_pool_t       *msg_pool;

	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** Media processing engine */
	mpf_engine_t              *media_engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** Signaling agent */
	mrcp_sig_agent_t          *signaling_agent;
	/** Connection agent */
	mrcp_connection_agent_t   *connection_agent;
};

/** Create client session */
mrcp_client_session_t* mrcp_client_session_create(mrcp_application_t *application, void *obj);
/** Create channel */
mrcp_channel_t* mrcp_client_channel_create(mrcp_session_t *session, mrcp_resource_id resource_id, mpf_termination_t *termination, void *obj);

/** Process application message */
apt_bool_t mrcp_client_app_message_process(mrcp_app_message_t *app_message);
/** Process MPF message */
apt_bool_t mrcp_client_mpf_message_process(mpf_message_t *mpf_message);

/** Process session answer */
apt_bool_t mrcp_client_session_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
/** Process session termination response */
apt_bool_t mrcp_client_session_terminate_response_process(mrcp_client_session_t *session);
/** Process session termination event */
apt_bool_t mrcp_client_session_terminate_event_process(mrcp_client_session_t *session);

/** Process channel modify event */
apt_bool_t mrcp_client_on_channel_modify(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor);
/** Process channel remove event */
apt_bool_t mrcp_client_on_channel_remove(mrcp_channel_t *channel);
/** Process message receive event */
apt_bool_t mrcp_client_on_message_receive(mrcp_client_session_t *session, mrcp_connection_t *connection, mrcp_message_t *message);

APT_END_EXTERN_C

#endif /*__MRCP_CLIENT_SESSION_H__*/
