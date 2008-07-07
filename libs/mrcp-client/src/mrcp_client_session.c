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

#include "mrcp_client_session.h"
#include "mrcp_resource.h"
#include "mrcp_resource_factory.h"
#include "mrcp_sig_agent.h"
#include "mrcp_client_connection.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_message.h"
#include "mpf_termination.h"
#include "mpf_engine.h"
#include "mpf_user.h"
#include "apt_consumer_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

typedef struct mrcp_termination_slot_t mrcp_termination_slot_t;

struct mrcp_termination_slot_t {
	/** waiting state */
	apt_bool_t         waiting;
	/** RTP termination */
	mpf_termination_t *termination;
};


void mrcp_client_session_add(mrcp_client_t *client, mrcp_client_session_t *session);
void mrcp_client_session_remove(mrcp_client_t *client, mrcp_client_session_t *session);

static apt_bool_t mrcp_client_session_offer_send(mrcp_client_session_t *session);

static apt_bool_t mrcp_client_application_response_send(mrcp_client_session_t *session, mrcp_app_status_code_e status);
static apt_bool_t mrcp_client_application_message_send(mrcp_client_session_t *session, mrcp_message_t *mrcp_message);
static apt_bool_t mrcp_client_application_request_dispatch(mrcp_client_session_t *session, const mrcp_app_message_t *app_message);

static apt_bool_t mrcp_client_control_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_client_av_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor);

static apt_bool_t mrcp_client_on_termination_add(mrcp_client_session_t *session, mpf_message_t *mpf_message);
static apt_bool_t mrcp_client_on_termination_modify(mrcp_client_session_t *session, mpf_message_t *mpf_message);
static apt_bool_t mrcp_client_on_termination_subtract(mrcp_client_session_t *session, mpf_message_t *mpf_message);

static mrcp_channel_t* mrcp_client_channel_find_by_id(mrcp_client_session_t *session, mrcp_resource_id resource_id);

static apt_bool_t mrcp_client_mpf_request_send(
						mrcp_client_session_t *session, 
						mpf_command_type_e command_id, 
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor);


mrcp_client_session_t* mrcp_client_session_create(mrcp_application_t *application, void *obj)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*) mrcp_session_create(sizeof(mrcp_client_session_t)-sizeof(mrcp_session_t));
	session->application = application;
	session->context = NULL;
	session->terminations = apr_array_make(session->base.pool,2,sizeof(mrcp_termination_slot_t));
	session->channels = apr_array_make(session->base.pool,2,sizeof(mrcp_channel_t*));
	session->offer = NULL;
	session->answer = NULL;
	session->active_request = NULL;
	session->request_queue = apt_list_create(session->base.pool);
	session->offer_flag_count = 0;
	session->answer_flag_count = 0;
	session->terminate_flag_count = 0;
	return session;
}

mrcp_channel_t* mrcp_client_channel_create(mrcp_session_t *session, mrcp_resource_id resource_id, mpf_termination_t *termination, void *obj)
{
	mrcp_channel_t *channel = apr_palloc(session->pool,sizeof(mrcp_channel_t));
	channel->pool = session->pool;
	channel->session = session;
	channel->resource_id = resource_id;
	channel->control_channel = NULL;
	channel->termination = NULL;
	channel->resource = NULL;
	channel->waiting = FALSE;
	return channel;
}

apt_bool_t mrcp_client_session_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	apt_log(APT_PRIO_INFO,"Receive Session Answer <%s> [c:%d a:%d v:%d]",
		session->base.id.buf ? session->base.id.buf : "new",
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);

	mrcp_client_control_media_answer_process(session,descriptor);
	mrcp_client_av_media_answer_process(session,descriptor);

	/* store received answer */
	session->answer = descriptor;

	if(!session->answer_flag_count) {
		/* send response to application */
		mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
	}

	return TRUE;
}

apt_bool_t mrcp_client_session_terminate_response_process(mrcp_client_session_t *session)
{
	apt_log(APT_PRIO_INFO,"Receive Session Termination Response <%s>",
		session->base.id.buf ? session->base.id.buf : "new");

	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
	}

	if(!session->terminate_flag_count) {
		mrcp_client_session_remove(session->application->client,session);
		/* send response to application */
		mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
	}
	return TRUE;
}

apt_bool_t mrcp_client_session_terminate_event_process(mrcp_client_session_t *session)
{
	if(session->active_request) {
		/* send response to application */
		mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_TERMINATE);
	}
	else {
		/* process event */
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_channel_modify(mrcp_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Modify");
	if(!channel->waiting) {
		return FALSE;
	}
	channel->waiting = TRUE;
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send response to application */
			mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_channel_remove(mrcp_channel_t *channel)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)channel->session;
	apt_log(APT_PRIO_DEBUG,"On Control Channel Remove");
	if(!channel->waiting) {
		return FALSE;
	}
	channel->waiting = TRUE;
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			mrcp_client_session_remove(session->application->client,session);
			/* send response to application */
			mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
		}
	}
	return TRUE;
}

apt_bool_t mrcp_client_on_message_receive(mrcp_client_session_t *session, mrcp_connection_t *connection, mrcp_message_t *message)
{
	mrcp_channel_t *channel = mrcp_client_channel_find_by_id(session,message->channel_id.resource_id);
	if(!channel) {
		apt_log(APT_PRIO_WARNING,"No such channel [%d]",message->channel_id.resource_id);
		return FALSE;
	}
	return mrcp_client_application_message_send(session,message);
}

apt_bool_t mrcp_client_app_message_process(mrcp_app_message_t *app_message)
{
	mrcp_client_session_t *session = (mrcp_client_session_t*)app_message->session;
	if(app_message->message_type != MRCP_APP_MESSAGE_TYPE_REQUEST) {
		return FALSE;
	}
	apt_log(APT_PRIO_INFO,"Receive Request from Application [%d]",app_message->command_id);
	if(session->active_request) {
		apt_log(APT_PRIO_DEBUG,"Push Request to Queue");
		apt_list_push_back(session->request_queue,app_message);
	}
	else {
		session->active_request = app_message;
		mrcp_client_application_request_dispatch(session,app_message);
	}
	return TRUE;
}

apt_bool_t mrcp_client_mpf_message_process(mpf_message_t *mpf_message)
{
	mrcp_client_session_t *session = NULL;
	if(mpf_message->termination) {
		session = mpf_termination_object_get(mpf_message->termination);
	}
	if(mpf_message->message_type == MPF_MESSAGE_TYPE_RESPONSE) {
		switch(mpf_message->command_id) {
			case MPF_COMMAND_ADD:
				apt_log(APT_PRIO_DEBUG,"On Termination Add");
				mrcp_client_on_termination_add(session,mpf_message);
				break;
			case MPF_COMMAND_MODIFY:
				apt_log(APT_PRIO_DEBUG,"On Termination Modify");
				mrcp_client_on_termination_modify(session,mpf_message);
				break;
			case MPF_COMMAND_SUBTRACT:
				apt_log(APT_PRIO_DEBUG,"On Termination Subtract");
				mrcp_client_on_termination_subtract(session,mpf_message);
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

static apt_bool_t mrcp_client_session_offer_send(mrcp_client_session_t *session)
{
	mrcp_session_descriptor_t *descriptor = session->offer;
	apt_log(APT_PRIO_INFO,"Send Session Offer <%s> [c:%d a:%d v:%d]",
		session->base.id.buf ? session->base.id.buf : "new",
		descriptor->control_media_arr->nelts,
		descriptor->audio_media_arr->nelts,
		descriptor->video_media_arr->nelts);
	return mrcp_session_offer(&session->base,descriptor);
}

static apt_bool_t mrcp_client_application_response_send(mrcp_client_session_t *session, mrcp_app_status_code_e status)
{
	mrcp_app_message_t *response;
	if(!session->active_request) {
		return FALSE;
	}
	response = apr_palloc(session->base.pool,sizeof(mrcp_app_message_t));
	*response = *session->active_request;
	response->message_type = MRCP_APP_MESSAGE_TYPE_RESPONSE;
	response->status = status;
	apt_log(APT_PRIO_INFO,"Send Response to Application [%d]", response->command_id);
	session->application->handler(response);

	session->active_request = apt_list_pop_front(session->request_queue);
	if(session->active_request) {
		mrcp_client_application_request_dispatch(session,session->active_request);
	}
	return TRUE;
}

static apt_bool_t mrcp_client_application_message_send(mrcp_client_session_t *session, mrcp_message_t *mrcp_message)
{
	mrcp_app_message_t *response;
	if(!session->active_request) {
		return FALSE;
	}
	response = apr_palloc(session->base.pool,sizeof(mrcp_app_message_t));
	*response = *session->active_request;
	response->message_type = MRCP_APP_MESSAGE_TYPE_RESPONSE;
	response->status = MRCP_APP_STATUS_CODE_SUCCESS;
	response->mrcp_message = mrcp_message;
	apt_log(APT_PRIO_INFO,"Send MRCP Message to Application");
	session->application->handler(response);

	session->active_request = apt_list_pop_front(session->request_queue);
	if(session->active_request) {
		mrcp_client_application_request_dispatch(session,session->active_request);
	}
	return TRUE;
}


static apt_bool_t mrcp_client_channel_find(mrcp_client_session_t *session, mrcp_channel_t *channel, int *index)
{
	int i;
	for(i=0; i<session->channels->nelts; i++) {
		mrcp_channel_t *existing_channel = ((mrcp_channel_t**)session->channels->elts)[i];
		if(existing_channel == channel) {
			if(index) {
				*index = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}

static mrcp_channel_t* mrcp_client_channel_find_by_id(mrcp_client_session_t *session, mrcp_resource_id resource_id)
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

static mrcp_termination_slot_t* mrcp_client_rtp_termination_find(mrcp_client_session_t *session, mpf_termination_t *termination)
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

static apt_bool_t mrcp_client_message_send(mrcp_client_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	apt_log(APT_PRIO_INFO,"Send MRCP Message to Server");
	if(!session->base.id.length) {
		mrcp_message_t *response = mrcp_response_create(message,message->pool);
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		apt_log(APT_PRIO_DEBUG,"Send Failed MRCP Message to Application");
		mrcp_client_application_message_send(session,response);
		return TRUE;
	}

	message->channel_id.session_id = session->base.id;
	mrcp_client_control_message_send(
		channel->control_channel,
		message);
	return TRUE;
}

static apt_bool_t mrcp_client_channel_modify(mrcp_client_session_t *session, mrcp_channel_t *channel, apt_bool_t enable)
{
	int index;
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Modify Control Channel [%d]",enable);
	if(mrcp_client_channel_find(session,channel,&index) == TRUE) {
		mrcp_control_descriptor_t *control_media = mrcp_session_control_media_get(session->offer,(apr_size_t)index);
		if(control_media) {
			control_media->port = (enable == TRUE) ? 9 : 0;
		}
	}

	return mrcp_client_session_offer_send(session);
}

static apt_bool_t mrcp_client_channel_add(mrcp_client_session_t *session, mrcp_channel_t *channel, mpf_rtp_termination_descriptor_t *rtp_descriptor)
{
	mrcp_channel_t **channel_slot;
	mrcp_control_descriptor_t *control_media;
	mrcp_termination_slot_t *termination_slot;
	mpf_termination_t *termination;
	const apt_str_t *resource_name;
	apr_pool_t *pool = session->base.pool;
	if(mrcp_client_channel_find(session,channel,NULL) == TRUE) {
		/* update */
		return mrcp_client_channel_modify(session,channel,TRUE);
	}

	if(!session->offer) {
		session->base.signaling_agent = session->application->signaling_agent;
		session->base.signaling_agent->create_client_session(&session->base);

		session->offer = mrcp_session_descriptor_create(pool);
		session->context = mpf_context_create(session,5,pool);
	}
	if(!channel->resource) {
		channel->resource = mrcp_resource_get(session->application->resource_factory,channel->resource_id);
		if(!channel->resource) {
			return FALSE;
		}
	}
	if(!channel->control_channel) {
		channel->control_channel = mrcp_client_control_channel_create(session->application->connection_agent,channel,pool);
	}

	/* add to channel array */
	apt_log(APT_PRIO_DEBUG,"Add Control Channel");
	channel_slot = apr_array_push(session->channels);
	*channel_slot = channel;
	
	/* create rtp termination */
	termination = mpf_termination_create(session->application->rtp_termination_factory,session,session->base.pool);
	/* add to channel array */
	apt_log(APT_PRIO_DEBUG,"Add RTP Termination");
	termination_slot = apr_array_push(session->terminations);
	termination_slot->waiting = FALSE;
	termination_slot->termination = termination;
	/* send add termination request (add to media context) */
	if(!rtp_descriptor) {
		rtp_descriptor = apr_palloc(pool,sizeof(mpf_rtp_termination_descriptor_t));
		mpf_rtp_termination_descriptor_init(rtp_descriptor);
	}
	if(mrcp_client_mpf_request_send(session,MPF_COMMAND_ADD,session->context,termination,rtp_descriptor) == TRUE) {
		termination_slot->waiting = TRUE;
		session->offer_flag_count++;
	}

	control_media = apr_palloc(pool,sizeof(mrcp_control_descriptor_t));
	mrcp_control_descriptor_init(control_media);
	control_media->id = mrcp_session_control_media_add(session->offer,control_media);
	control_media->cmid = session->offer->control_media_arr->nelts;

	control_media->proto = MRCP_PROTO_TCP;
	control_media->port = 9;
	control_media->setup_type = MRCP_SETUP_TYPE_ACTIVE;
	control_media->connection_type = MRCP_CONNECTION_TYPE_EXISTING;
	resource_name = mrcp_resource_name_get(session->application->resource_factory,channel->resource_id,session->application->version);
	if(resource_name) {
		control_media->resource_name = *resource_name;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_session_update(mrcp_client_session_t *session)
{
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Update Session");
	return mrcp_client_session_offer_send(session);
}

static apt_bool_t mrcp_client_session_terminate(mrcp_client_session_t *session)
{
	mrcp_channel_t *channel;
	mrcp_termination_slot_t *slot;
	int i;
	if(!session->offer) {
		return FALSE;
	}
	apt_log(APT_PRIO_DEBUG,"Terminate Session");
	/* remove existing control channels */
	for(i=0; i<session->channels->nelts; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* remove channel */
		apt_log(APT_PRIO_DEBUG,"Remove Control Channel");
		if(mrcp_client_control_channel_remove(channel->control_channel) == TRUE) {
			channel->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}

	/* subtract existing terminations */
	for(i=0; i<session->terminations->nelts; i++) {
		/* get existing termination */
		slot = &((mrcp_termination_slot_t*)session->terminations->elts)[i];
		if(!slot || !slot->termination) continue;

		/* send subtract termination request */
		apt_log(APT_PRIO_DEBUG,"Subtract Termination");
		if(mrcp_client_mpf_request_send(session,MPF_COMMAND_SUBTRACT,session->context,slot->termination,NULL) == TRUE) {
			slot->waiting = TRUE;
			session->terminate_flag_count++;
		}
	}

	session->terminate_flag_count++;
	mrcp_session_terminate_request(&session->base);
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_add(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	mpf_rtp_termination_descriptor_t *rtp_descriptor;
	mrcp_termination_slot_t *termination_slot;
	if(!session || !session->offer) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(!termination_slot || termination_slot->waiting == FALSE) {
		return FALSE;
	}
	termination_slot->waiting = FALSE;
	
	rtp_descriptor = mpf_message->descriptor;
	if(rtp_descriptor->audio.local) {
		session->offer->ip = rtp_descriptor->audio.local->base.ip;
		rtp_descriptor->audio.local->base.id = mrcp_session_audio_media_add(session->offer,rtp_descriptor->audio.local);
		rtp_descriptor->audio.local->mid = session->offer->audio_media_arr->nelts;
	}
	if(session->offer_flag_count) {
		session->offer_flag_count--;
		if(!session->offer_flag_count) {
			/* send offer to server */
			mrcp_client_session_offer_send(session);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_modify(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	mpf_rtp_termination_descriptor_t *rtp_descriptor;
	mrcp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(!termination_slot || termination_slot->waiting == FALSE) {
		return FALSE;
	}
	termination_slot->waiting = FALSE;
	rtp_descriptor = mpf_message->descriptor;

	if(session->offer_flag_count) {
		session->offer_flag_count--;
		if(!session->offer_flag_count) {
			/* send offer to server */
			mrcp_client_session_offer_send(session);
		}
	}
	if(session->answer_flag_count) {
		session->answer_flag_count--;
		if(!session->answer_flag_count) {
			/* send response to application */
			mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
		}
	}
	
	return TRUE;
}

static apt_bool_t mrcp_client_on_termination_subtract(mrcp_client_session_t *session, mpf_message_t *mpf_message)
{
	mrcp_termination_slot_t *termination_slot;
	if(!session) {
		return FALSE;
	}
	termination_slot = mrcp_client_rtp_termination_find(session,mpf_message->termination);
	if(!termination_slot || termination_slot->waiting == FALSE) {
		return FALSE;
	}
	termination_slot->waiting = FALSE;
	if(session->terminate_flag_count) {
		session->terminate_flag_count--;
		if(!session->terminate_flag_count) {
			mrcp_client_session_remove(session->application->client,session);
			/* send response to application */
			mrcp_client_application_response_send(session,MRCP_APP_STATUS_CODE_SUCCESS);
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_control_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_channel_t *channel;
	mrcp_control_descriptor_t *control_descriptor;
	int i;
	int count = session->channels->nelts;
	if(count != descriptor->control_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of control channels [%d] != Number of control media in answer [%d]",
			count,descriptor->control_media_arr->nelts);
		count = descriptor->control_media_arr->nelts;
	}

	if(!session->base.id.length) {
		/* initial answer received, store session id and add to session's table */
		control_descriptor = mrcp_session_control_media_get(descriptor,0);
		if(control_descriptor) {
			session->base.id = control_descriptor->session_id;
			mrcp_client_session_add(session->application->client,session);
		}
	}

	/* update existing control channels */
	for(i=0; i<count; i++) {
		/* get existing channel */
		channel = *((mrcp_channel_t**)session->channels->elts + i);
		if(!channel) continue;

		/* get control descriptor */
		control_descriptor = mrcp_session_control_media_get(descriptor,i);
		/* modify channel */
		apt_log(APT_PRIO_DEBUG,"Modify Control Channel");
		if(mrcp_client_control_channel_modify(channel->control_channel,control_descriptor) == TRUE) {
			channel->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_av_media_answer_process(mrcp_client_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	mrcp_termination_slot_t *slot;
	int i;
	int count = session->terminations->nelts;
	if(count != descriptor->audio_media_arr->nelts) {
		apt_log(APT_PRIO_WARNING,"Number of terminations [%d] != Number of audio media in answer [%d]",
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
		apt_log(APT_PRIO_DEBUG,"Modify Termination");
		if(mrcp_client_mpf_request_send(session,MPF_COMMAND_MODIFY,session->context,slot->termination,rtp_descriptor) == TRUE) {
			slot->waiting = TRUE;
			session->answer_flag_count++;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_client_application_request_dispatch(mrcp_client_session_t *session, const mrcp_app_message_t *app_message)
{
	apt_log(APT_PRIO_DEBUG,"Dispatch Application Request [%d]",app_message->command_id);
	switch(app_message->command_id) {
		case MRCP_APP_COMMAND_SESSION_UPDATE:
			mrcp_client_session_update(session);
			break;
		case MRCP_APP_COMMAND_SESSION_TERMINATE:
			mrcp_client_session_terminate(session);
			break;
		case MRCP_APP_COMMAND_CHANNEL_ADD:
			mrcp_client_channel_add(session,app_message->channel,app_message->descriptor);
			break;
		case MRCP_APP_COMMAND_CHANNEL_REMOVE:
			mrcp_client_channel_modify(session,app_message->channel,FALSE);
			break;
		case MRCP_APP_COMMAND_MESSAGE:
			mrcp_client_message_send(session,app_message->channel,app_message->mrcp_message);
			break;
		default:
			break;
	}
	return TRUE;
}

static apt_bool_t mrcp_client_mpf_request_send(
						mrcp_client_session_t *session, 
						mpf_command_type_e command_id,
						mpf_context_t *context, 
						mpf_termination_t *termination, 
						void *descriptor)
{
	apt_task_t *media_task = mpf_task_get(session->application->media_engine);
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
