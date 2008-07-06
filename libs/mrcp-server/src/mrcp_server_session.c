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
#include "mrcp_resource.h"
#include "mrcp_resource_factory.h"
#include "mrcp_resource_engine.h"
#include "mrcp_sig_agent.h"
#include "mrcp_server_connection.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_message.h"
#include "mpf_user.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

#define MRCP_SESSION_ID_HEX_STRING_LENGTH 16

struct mrcp_channel_t {
	/** Memory pool */
	apr_pool_t            *pool;
	/** MRCP resource */
	mrcp_resource_t       *resource;
	/** MRCP session entire channel belongs to */
	mrcp_session_t        *session;
	/** MRCP connection */
	mrcp_connection_t     *connection;

	/** MRCP resource engine channel */
	mrcp_engine_channel_t *engine_channel;
	/** waiting state */
	apt_bool_t             waiting;
};

typedef struct mrcp_termination_slot_t mrcp_termination_slot_t;

struct mrcp_termination_slot_t {
	/** RTP termination */
	mpf_termination_t *termination;
	/** waiting state */
	apt_bool_t         waiting;
};

extern const mrcp_engine_channel_event_vtable_t engine_channel_vtable;

void mrcp_server_session_add(mrcp_server_session_t *session);
void mrcp_server_session_remove(mrcp_server_session_t *session);

static apt_bool_t mrcp_server_control_media_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_server_av_media_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor);

static apt_bool_t mrcp_server_on_termination_modify(mrcp_server_session_t *session, const mpf_message_t *mpf_message);
static apt_bool_t mrcp_server_on_termination_subtract(mrcp_server_session_t *session, const mpf_message_t *mpf_message);

static apt_bool_t mrcp_server_session_answer_send(mrcp_server_session_t *session);

static mrcp_channel_t* mrcp_server_channel_find_by_id(mrcp_server_session_t *session, mrcp_resource_id resource_id);

static apt_bool_t mrcp_server_mpf_request_send(
						mrcp_server_session_t *session, 
						mpf_command_type_e command_id, 
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor);


mrcp_server_session_t* mrcp_server_session_create()
{
	mrcp_server_session_t *session = (mrcp_server_session_t*) mrcp_session_create(sizeof(mrcp_server_session_t)-sizeof(mrcp_session_t));
	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mrcp_termination_slot_t));
	session->channels = apr_array_make(session->base.pool,2,sizeof(mrcp_channel_t*));
	session->offer = NULL;
	session->answer = NULL;
	session->answer_flag_count = 0;
	session->terminate_flag_count = 0;
	return session;
}

static mrcp_engine_channel_t* mrcp_server_engine_channel_create(mrcp_server_session_t *session, mrcp_resource_id resource_id)
{
	mrcp_resource_engine_t *resource_engine;
	apt_list_elem_t *elem = apt_list_first_elem_get(session->resource_engines);
	/* walk through the list of engines */
	while(elem) {
		resource_engine = apt_list_elem_object_get(elem);
		if(resource_engine && resource_engine->resource_id == resource_id) {
			return resource_engine->method_vtable->create_channel(resource_engine,session->base.pool);
		}
		elem = apt_list_next_elem_get(session->resource_engines,elem);
	}
	return NULL;
}

static mrcp_channel_t* mrcp_server_channel_create(mrcp_server_session_t *session, apt_str_t *resource_name)
{
	mrcp_resource_id resource_id;
	mrcp_resource_t *resource;
	mrcp_engine_channel_t *engine_channel;
	mrcp_channel_t *channel;

	resource_id = mrcp_resource_id_find(session->resource_factory,resource_name,session->base.signaling_agent->mrcp_version);
	resource = mrcp_resource_get(session->resource_factory,resource_id);
	if(!resource) {
		return NULL;
	}

	engine_channel = mrcp_server_engine_channel_create(session,resource_id);
	if(!engine_channel) {
		return NULL;
	}

	channel = apr_palloc(session->base.pool,sizeof(mrcp_channel_t));
	channel->pool = session->base.pool;
	channel->resource = resource;
	channel->session = &session->base;
	channel->connection = NULL;
	channel->engine_channel = engine_channel;
	channel->waiting = FALSE;

	engine_channel->event_obj = channel;
	engine_channel->event_vtable = &engine_channel_vtable;
	return channel;
}

mrcp_session_t* mrcp_server_channel_session_get(mrcp_channel_t *channel)
{
	return channel->session;
}

apt_bool_t mrcp_server_session_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	if(session->offer) {
		/* last offer received is still in-progress, new offer is not allowed */
		apt_log(APT_PRIO_WARNING,"Cannot Accept New Offer");
		return FALSE;
	}
	if(!session->base.id.length) {
		/* initial offer received, generate session id and add to session's table */
		apt_unique_id_generate(&session->base.id,MRCP_SESSION_ID_HEX_STRING_LENGTH,session->base.pool);
		mrcp_server_session_add(session);

		session->context = mpf_context_create(session,5,session->base.pool);
	}
	apt_log(APT_PRIO_INFO,"Receive Session Offer <%s> [c:%d a:%d v:%d]",
		session->base.id.buf,
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);

	mrcp_server_control_media_offer_process(session,descriptor);
	mrcp_server_av_media_offer_process(session,descriptor);

	/* store received offer */
	session->offer = descriptor;
	session->answer = mrcp_session_descriptor_create(session->base.pool);
	return TRUE;
}

apt_bool_t mrcp_server_session_terminate_process(mrcp_server_session_t *session)
{
	mrcp_channel_t *channel;
	mrcp_termination_slot_t *slot;
	int i;
	apt_log(APT_PRIO_INFO,"Receive Session Termination Request");
	for(i=0; i<session->channels->nelts; i++) {
		channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(!channel) continue;

		/* send remove channel request */
		apt_log(APT_PRIO_DEBUG,"Remove Control Channel");
		if(mrcp_server_connection_remove(session->connection_agent,channel,channel->connection,channel->pool) == TRUE) {
			channel->waiting = TRUE;
			session->terminate_flag_count++;
		}
		/* close resource engine channel */
		if(mrcp_engine_channel_close(channel->engine_channel) == TRUE) {
			session->terminate_flag_count++;
		}
	}
	for(i=0; i<session->terminations->nelts; i++) {
		/* get existing termination */
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* send subtract termination request */
		apt_log(APT_PRIO_DEBUG,"Subtract Termination");
		if(mrcp_server_mpf_request_send(session,MPF_COMMAND_SUBTRACT,session->context,slot->termination,NULL) == TRUE) {
			slot->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}
	mrcp_server_session_remove(session);
	return TRUE;
}

apt_bool_t mrcp_server_on_channel_modify(mrcp_channel_t *channel, mrcp_connection_t *connection, mrcp_control_descriptor_t *answer)
{
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Modify");
	if(!answer) {
		return FALSE;
	}
	if(!channel->waiting) {
		return FALSE;
	}
	channel->waiting = TRUE;
	channel->connection = connection;
	answer->session_id = session->base.id;
	mrcp_session_control_media_add(session->answer,answer);
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send answer to client */
			mrcp_server_session_answer_send(session);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_on_channel_remove(mrcp_channel_t *channel)
{
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Remove");
	if(!channel->waiting) {
		return FALSE;
	}
	channel->waiting = TRUE;
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			/* send termination response to client */
			mrcp_session_terminate_response(&session->base);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_on_message_receive(mrcp_server_session_t *session, mrcp_connection_t *connection, mrcp_message_t *message)
{
	mrcp_channel_t *channel = mrcp_server_channel_find_by_id(session,message->channel_id.resource_id);
	if(!channel) {
		apt_log(APT_PRIO_WARNING,"No such channel [%d]",message->channel_id.resource_id);
		return FALSE;
	}

	/* update state machine */

	/* send message to resource engine for actual processing */
	return mrcp_engine_channel_request_process(channel->engine_channel,message);
}

apt_bool_t mrcp_server_on_engine_channel_open(mrcp_channel_t *channel, apt_bool_t status)
{
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Engine Channel Open");
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send answer to client */
			mrcp_server_session_answer_send(session);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_on_engine_channel_close(mrcp_channel_t *channel)
{
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Engine Channel Close");
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			/* send termination response to client */
			mrcp_session_terminate_response(&session->base);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_server_on_engine_channel_message(mrcp_channel_t *channel, mrcp_message_t *message)
{
	mrcp_server_session_t *session = (mrcp_server_session_t*)channel->session;

	/* update state machine */

	/* send response/event message to client */
	return mrcp_server_connection_message_send(
				session->connection_agent,
				channel->connection,
				message);
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
		if(mrcp_server_connection_modify(session->connection_agent,channel,channel->connection,control_descriptor,channel->pool) == TRUE) {
			channel->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	
	/* add new control channels */
	for(; i<descriptor->control_media_arr->nelts; i++) {
		mrcp_channel_t **slot;
		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		if(!control_descriptor) continue;

		/* create new MRCP channel instance */
		channel = mrcp_server_channel_create(session,&control_descriptor->resource_name);
		if(!channel) continue;
		/* add to channel array */
		apt_log(APT_PRIO_DEBUG,"Add Control Channel");
		slot = apr_array_push(session->channels);
		*slot = channel;

		/* send modify connection request */
		if(mrcp_server_connection_modify(session->connection_agent,channel,channel->connection,control_descriptor,channel->pool) == TRUE) {
			channel->waiting = TRUE;
			session->answer_flag_count++;
		}
		/* open resource engine channel */
		if(mrcp_engine_channel_open(channel->engine_channel) == TRUE) {
			session->answer_flag_count++;
		}
	}

	return TRUE;
}

static apt_bool_t mrcp_server_av_media_offer_process(mrcp_server_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_termination_slot_t *slot;
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
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send modify termination request */
		apt_log(APT_PRIO_DEBUG,"Modify RTP Termination");
		if(mrcp_server_mpf_request_send(session,MPF_COMMAND_MODIFY,session->context,slot->termination,rtp_descriptor) == TRUE) {
			slot->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	
	/* add new terminations */
	for(; i<descriptor->audio_media_arr->nelts; i++) {
		mpf_rtp_termination_descriptor_t *rtp_descriptor;
		mpf_termination_t *termination;
		/* create new RTP termination instance */
		termination = mpf_termination_create(session->rtp_termination_factory,session,session->base.pool);
		/* add to termination array */
		apt_log(APT_PRIO_DEBUG,"Add RTP Termination");
		slot = apr_array_push(session->terminations);
		slot->waiting = FALSE;
		slot->termination = termination;

		/* construct termination descriptor */
		rtp_descriptor = apr_palloc(session->base.pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
		rtp_descriptor->audio.local = NULL;
		rtp_descriptor->audio.remote = mrcp_session_audio_media_get(descriptor,i);

		/* send add termination request (add to media context) */
		if(mrcp_server_mpf_request_send(session,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor) == TRUE) {
			slot->waiting = TRUE;
			session->answer_flag_count++;
		}
	}

	return TRUE;
}

static apt_bool_t mrcp_server_session_answer_send(mrcp_server_session_t *session)
{
	apt_bool_t status;
	mrcp_session_descriptor_t *descriptor = session->answer;
	apt_log(APT_PRIO_INFO,"Send Session Answer <%s> [c:%d a:%d v:%d]",
		session->base.id.buf,
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);
	status = mrcp_session_answer(&session->base,descriptor);
	session->offer = NULL;
	session->answer = NULL;
	return status;
}

static mrcp_channel_t* mrcp_server_channel_find_by_id(mrcp_server_session_t *session, mrcp_resource_id resource_id)
{
	int i;
	mrcp_channel_t *channel;
	for(i=0; i<session->channels->nelts; i++) {
		channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(channel && channel->resource && channel->resource->id == resource_id) {
			return channel;
		}
	}
	return NULL;
}

static mrcp_termination_slot_t* mrcp_server_rtp_termination_find(mrcp_server_session_t *session, mpf_termination_t *termination)
{
	int i;
	mrcp_termination_slot_t *slot;
	for(i=0; i<session->terminations->nelts; i++) {
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(slot && slot->termination == termination) {
			return slot;
		}
	}
	return NULL;
}

static apt_bool_t mrcp_server_on_termination_modify(mrcp_server_session_t *session, const mpf_message_t *mpf_message)
{
	mpf_rtp_termination_descriptor_t *rtp_descriptor;
	mrcp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_server_rtp_termination_find(session,mpf_message->termination);
	if(!termination_slot || termination_slot->waiting == FALSE) {
		return FALSE;
	}
	termination_slot->waiting = FALSE;
	rtp_descriptor = mpf_message->descriptor;
	if(rtp_descriptor->audio.local) {
		session->answer->ip = rtp_descriptor->audio.local->base.ip;
		mrcp_session_audio_media_add(session->answer,rtp_descriptor->audio.local);
	}
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send answer to client */
			mrcp_server_session_answer_send(session);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_on_termination_subtract(mrcp_server_session_t *session, const mpf_message_t *mpf_message)
{
	mrcp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_server_rtp_termination_find(session,mpf_message->termination);
	if(!termination_slot || termination_slot->waiting == FALSE) {
		return FALSE;
	}
	termination_slot->waiting = FALSE;
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			/* send response to client */
			mrcp_session_terminate_response(&session->base);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_mpf_request_send(
						mrcp_server_session_t *session, 
						mpf_command_type_e command_id, 
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor)
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
