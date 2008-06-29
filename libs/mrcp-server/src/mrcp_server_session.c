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

#include "mrcp_server.h"
#include "mrcp_server_session.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_server_connection.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mpf_user.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define MRCP_SESSION_ID_HEX_STRING_LENGTH 16

struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t        *pool;
	/** MRCP resource */
	mrcp_resource_t   *resource;
	/** MRCP session entire channel belongs to (added for fast reverse search) */
	mrcp_session_t    *session;
	/** MRCP connection */
	mrcp_connection_t *connection;

	/** Media termination */
	mpf_termination_t *termination;
};


void mrcp_server_session_add(mrcp_server_session_t *session);
void mrcp_server_session_remove(mrcp_server_session_t *session);

static apt_bool_t mpf_request_send(mrcp_server_session_t *session, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor);


mrcp_server_session_t* mrcp_server_session_create()
{
	mrcp_server_session_t *session = (mrcp_server_session_t*) mrcp_session_create(sizeof(mrcp_server_session_t)-sizeof(mrcp_session_t));
	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mpf_termination_t*));
	session->channels = apr_array_make(session->base.pool,2,sizeof(mrcp_channel_t*));
	session->offer = NULL;
	session->answer = NULL;
	return session;
}

static mrcp_channel_t* mrcp_server_channel_create(mrcp_session_t *session, apt_str_t *resource_name)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->session = session;
	channel->connection = NULL;
	channel->termination = NULL;
	channel->resource = NULL;
	return channel;
}

static apt_bool_t mrcp_server_control_media_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_channel_t *channel;
	mrcp_control_descriptor_t *control_descriptor;
	int i;
	int count = session->channels->nelts;
	if(count > descriptor->control_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of control channels [%d] > Number of control media in offer [%d]",
			count,descriptor->control_media_arr->nelts);
		count = descriptor->control_media_arr->nelts;
	}
	
	/* update existing control channels */
	for(i=0; i<count; i++) {
		/* get existing termination */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		/* send offer */
		apt_log(APT_PRIO_DEBUG,"Modify Control Channel");
		mrcp_server_connection_modify(session->connection_agent,channel,channel->connection,control_descriptor,channel->pool);
	}
	
	/* add new control channels */
	for(; i<descriptor->control_media_arr->nelts; i++) {
		mrcp_channel_t **slot;
		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		if(!control_descriptor) continue;

		/* create new MRCP channel instance */
		channel = mrcp_server_channel_create(&session->base,&control_descriptor->resource_name);
		/* add to channel array */
		apt_log(APT_PRIO_DEBUG,"Add Control Channel");
		slot = apr_array_push(session->channels);
		*slot = channel;

		/* send offer */
		mrcp_server_connection_modify(session->connection_agent,channel,channel->connection,control_descriptor,channel->pool);
	}

	return TRUE;
}

static apt_bool_t mrcp_server_av_media_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mpf_termination_t *termination;
	int i;
	int count = session->terminations->nelts;
	if(count > descriptor->audio_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of terminations [%d] > Number of audio media in offer [%d]\n",
			count,descriptor->audio_media_arr->nelts);
		count = descriptor->audio_media_arr->nelts;
	}
	
	/* update existing terminations */
	for(i=0; i<count; i++) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		/* get existing termination */
		termination = *((mpf_termination_t**)session->terminations->elts + i);
		if(!termination) continue;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send modify termination request */
		apt_log(APT_PRIO_DEBUG,"Modify Termination");
		mpf_request_send(session,MPF_COMMAND_MODIFY,session->context,termination,rtp_descriptor);
	}
	
	/* add new terminations */
	for(; i<descriptor->audio_media_arr->nelts; i++) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		mpf_termination_t **slot;
		/* create new RTP termination instance */
		termination = mpf_termination_create(session->rtp_termination_factory,session,session->base.pool);
		/* add to termination array */
		apt_log(APT_PRIO_DEBUG,"Add Termination");
		slot = apr_array_push(session->terminations);
		*slot = termination;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send add termination request (add to media context) */
		mpf_request_send(session,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor);
	}

	return TRUE;
}

apt_bool_t mrcp_server_session_offer_process(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_server_session_t *server_session = (mrcp_server_session_t*)session;
	if(server_session->offer) {
		/* last offer received is still in-progress, new offer is not allowed */
		apt_log(APT_PRIO_WARNING,"Cannot Accept New Offer");
		return FALSE;
	}
	if(!session->id.length) {
		/* initial offer received, generate session id and add to session's table */
		apt_unique_id_generate(&session->id,MRCP_SESSION_ID_HEX_STRING_LENGTH,session->pool);
		mrcp_server_session_add(server_session);

		server_session->context = mpf_context_create(server_session,5,session->pool);
	}
	apt_log(APT_PRIO_INFO,"Process Session Offer <%s> [c:%d a:%d v:%d]",
		session->id.buf,
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);

	mrcp_server_control_media_offer_process(server_session,descriptor);
	mrcp_server_av_media_offer_process(server_session,descriptor);

	/* store received offer */
	server_session->offer = descriptor;
	server_session->answer = mrcp_session_descriptor_create(session->pool);
	return TRUE;
}

apt_bool_t mrcp_server_session_terminate_process(mrcp_session_t *session)
{
	int i;
	mrcp_server_session_t *server_session = (mrcp_server_session_t*)session;
	apt_log(APT_PRIO_INFO,"Process Session Terminate");
	for(i=0; i<server_session->channels->nelts; i++) {
		mrcp_channel_t *channel = ((mrcp_channel_t**)server_session->channels->elts)[i];
		if(channel) {
			/* send remove channel request */
			apt_log(APT_PRIO_DEBUG,"Remove Control Channel");
			mrcp_server_connection_remove(server_session->connection_agent,channel,channel->connection,channel->pool);
		}
	}
	for(i=0; i<server_session->terminations->nelts; i++) {
		mpf_termination_t *termination = ((mpf_termination_t**)server_session->terminations->elts)[i];
		if(termination) {
			/* send subtract termination request */
			apt_log(APT_PRIO_DEBUG,"Subtract Termination");
			mpf_request_send(server_session,MPF_COMMAND_SUBTRACT,server_session->context,termination,NULL);
		}
	}
	mrcp_server_session_remove(server_session);
	return TRUE;
}

static apt_bool_t mrcp_server_session_answer_is_ready(const mrcp_session_descriptor_t *offer, const mrcp_session_descriptor_t *answer)
{
	if(!offer || !answer) {
		return FALSE;
	}
	return	(offer->control_media_arr->nelts == answer->control_media_arr->nelts &&
			 offer->audio_media_arr->nelts == answer->audio_media_arr->nelts &&
			 offer->video_media_arr->nelts == answer->video_media_arr->nelts);
}

static apt_bool_t mrcp_server_session_answer_send(mrcp_server_session_t *session)
{
	apt_bool_t status = mrcp_session_answer(&session->base,session->answer);
	session->offer = NULL;
	session->answer = NULL;
	return status;
}

apt_bool_t mrcp_server_on_channel_modify(mrcp_channel_t *channel, mrcp_connection_t *connection, mrcp_control_descriptor_t *answer)
{
	apt_log(APT_PRIO_DEBUG,"On Control Channel Modify");
	if(answer) {
		mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
		channel->connection = connection;
		answer->session_id = session->base.id;
		
		mrcp_session_control_media_add(session->answer,answer);

		if(mrcp_server_session_answer_is_ready(session->offer,session->answer) == TRUE) {
			mrcp_server_session_answer_send(session);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_on_channel_remove(mrcp_channel_t *channel)
{
	int i;
	apt_bool_t empty = TRUE;
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
	if(!session) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"On Control Channel Remove");
	for(i=0; i<session->channels->nelts; i++) {
		mrcp_channel_t *existing_channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(!existing_channel) continue;

		if(existing_channel == channel) {
			((mrcp_channel_t**)session->channels->elts)[i] = NULL;
		}
		else {
			empty = FALSE;
		}
	}
	if(empty == TRUE) {
		for(i=0; i<session->terminations->nelts; i++) {
			mpf_termination_t *termination = ((mpf_termination_t**)session->terminations->elts)[i];
			if(termination) {
				empty = FALSE;
				break;
			}
		}
		if(empty == TRUE) {
			mrcp_session_terminate_response(&session->base);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_on_termination_modify(mrcp_server_session_t *session, const mpf_message_t *mpf_message)
{
	apt_log(APT_PRIO_DEBUG,"On Termination Modify");
	if(session && session->answer) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor = mpf_message->descriptor;
		if(rtp_descriptor->audio.local) {
			session->answer->ip = rtp_descriptor->audio.local->base.ip;
			mrcp_session_audio_media_add(session->answer,rtp_descriptor->audio.local);

			if(mrcp_server_session_answer_is_ready(session->offer,session->answer) == TRUE) {
				mrcp_server_session_answer_send(session);
			}
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_on_termination_subtract(mrcp_server_session_t *session, const mpf_message_t *mpf_message)
{
	int i;
	apt_bool_t empty = TRUE;
	apt_log(APT_PRIO_DEBUG,"On Termination Subtract");
	for(i=0; i<session->terminations->nelts; i++) {
		mpf_termination_t *termination = ((mpf_termination_t**)session->terminations->elts)[i];
		if(!termination) continue;

		if(termination == mpf_message->termination) {
			((mpf_termination_t**)session->terminations->elts)[i] = NULL;
		}
		else {
			empty = FALSE;
		}
	}
	if(empty == TRUE) {
		for(i=0; i<session->channels->nelts; i++) {
			mrcp_channel_t *channel = ((mrcp_channel_t**)session->channels->elts)[i];
			if(channel) {
				empty = FALSE;
				break;
			}
		}
		if(empty == TRUE) {
			mrcp_session_terminate_response(&session->base);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_mpf_message_process(mpf_message_t *mpf_message)
{
	mrcp_server_session_t *session = NULL;
	if(mpf_message->termination) {
		session = mpf_termination_object_get(mpf_message->termination);
	}
	if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
		switch(mpf_message->command_id) {
			case MPF_COMMAND_ADD:
				apt_log(APT_PRIO_DEBUG,"On Termination Add");
				mrcp_server_on_termination_modify(session,mpf_message);
				break;
			case MPF_COMMAND_MODIFY:
				apt_log(APT_PRIO_DEBUG,"On Termination Modify");
				mrcp_server_on_termination_modify(session,mpf_message);
				break;
			case MPF_COMMAND_SUBTRACT:
				apt_log(APT_PRIO_DEBUG,"On Termination Subtract");
				mrcp_server_on_termination_subtract(session,mpf_message);
				break;
			default:
				break;
		}
	}
	else if(mpf_message->message_type == MPF_MESSAGE_TYPE_EVENT) {
		apt_log(APT_PRIO_DEBUG,"Process MPF Event");
	}
	return TRUE;
}

static apt_bool_t mpf_request_send(mrcp_server_session_t *session, mpf_command_type_e command_id, 
				mpf_context_t *context, mpf_termination_t *termination, void *descriptor)
{
	apt_task_t *media_task = mpf_task_get(session->media_engine);
	apt_task_msg_t *msg;
	mpf_message_t *mpf_message;
	msg = apt_task_msg_get(media_task);
	msg->type = TASK_MSG_USER;
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = command_id;
	mpf_message->context = context;
	mpf_message->termination = termination;
	mpf_message->descriptor = descriptor;
	return apt_task_msg_signal(media_task,msg);
}
