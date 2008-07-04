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

#ifndef __MRCP_SERVER_SESSION_H__
#define __MRCP_SERVER_SESSION_H__

/**
 * @file mrcp_server_session.h
 * @brief MRCP Server Session
 */ 

#include "mrcp_session.h"
#include "mpf_message.h"
#include "apt_task.h"
#include "apt_obj_list.h"


APT_BEGIN_EXTERN_C

typedef struct mrcp_channel_t mrcp_channel_t;
typedef struct mrcp_server_session_t mrcp_server_session_t;

struct mrcp_server_session_t {
	/** Session base */
	mrcp_session_t             base;

	mrcp_server_t             *server;
	/** MRCP resource factory */
	mrcp_resource_factory_t   *resource_factory;
	/** MRCP resource engine list */
	apt_obj_list_t            *resource_engines;
	/** Media processing engine */
	mpf_engine_t              *media_engine;
	/** RTP termination factory */
	mpf_termination_factory_t *rtp_termination_factory;
	/** Connection agent */
	mrcp_connection_agent_t   *connection_agent;


	/** Media context */
	mpf_context_t             *context;

	/** Media termination array */
	apr_array_header_t        *terminations;
	/** MRCP control channel array */
	apr_array_header_t        *channels;

	/** In-progress offer */
	mrcp_session_descriptor_t *offer;
	/** In-progres answer */
	mrcp_session_descriptor_t *answer;

	apr_size_t                 answer_flag_count;
	apr_size_t                 terminate_flag_count;
};


mrcp_server_session_t* mrcp_server_session_create();

apt_bool_t mrcp_server_session_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor);
apt_bool_t mrcp_server_session_terminate_process(mrcp_server_session_t *session);

apt_bool_t mrcp_server_on_channel_modify(mrcp_channel_t *channel, mrcp_connection_t *connection, mrcp_control_descriptor_t *answer);
apt_bool_t mrcp_server_on_channel_remove(mrcp_channel_t *channel);
apt_bool_t mrcp_server_on_message_receive(mrcp_server_session_t *session, mrcp_connection_t *connection, mrcp_message_t *message);

apt_bool_t mrcp_server_mpf_message_process(mpf_message_t *mpf_message);


APT_END_EXTERN_C

#endif /*__MRCP_SERVER_SESSION_H__*/
