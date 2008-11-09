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
#include "demo_util.h"
#include "mrcp_session.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"
#include "mpf_termination.h"
#include "mpf_stream.h"

typedef struct synth_app_channel_t synth_app_channel_t;

/** Declaration of synthesizer application channel */
struct synth_app_channel_t {
	/** MRCP control channel */
	mrcp_channel_t     *channel;
	/** Audio stream */
	mpf_audio_stream_t *audio_stream;
	/** File to write audio stream to */
	FILE               *audio_out;
};

/** Declaration of synthesizer application methods */
static apt_bool_t synth_application_run(demo_application_t *demo_application, const char *profile);
static apt_bool_t synth_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message);

/** Declaration of application message handlers */
static apt_bool_t synth_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status);
static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message);

static const mrcp_app_message_dispatcher_t synth_application_dispatcher = {
	synth_application_on_session_update,
	synth_application_on_session_terminate,
	synth_application_on_channel_add,
	synth_application_on_channel_remove,
	synth_application_on_message_receive
};

/** Declaration of synthesizer audio stream methods */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream);
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	synth_app_stream_destroy,
	NULL,
	NULL,
	NULL,
	synth_app_stream_open,
	synth_app_stream_close,
	synth_app_stream_write
};


/** Create demo synthesizer application */
demo_application_t* demo_synth_application_create(apr_pool_t *pool)
{
	demo_application_t *synth_application = apr_palloc(pool,sizeof(demo_application_t));
	synth_application->application = NULL;
	synth_application->framework = NULL;
	synth_application->handler = synth_application_handler;
	synth_application->run = synth_application_run;
	return synth_application;
}

/** Create demo synthesizer channel */
static mrcp_channel_t* synth_application_channel_create(mrcp_session_t *session)
{
	mrcp_channel_t *channel;
	mpf_termination_t *termination;
	/* create channel */
	synth_app_channel_t *synth_channel = apr_palloc(session->pool,sizeof(synth_app_channel_t));
	synth_channel->audio_out = NULL;
	/* create audio stream */
	synth_channel->audio_stream = mpf_audio_stream_create(
			synth_channel,        /* object to associate */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			STREAM_MODE_SEND,     /* stream mode/direction */
			session->pool);       /* memory pool to allocate memory from */
	/* create raw termination */
	termination = mpf_raw_termination_create(
			NULL,                        /* no object to associate */
			synth_channel->audio_stream, /* audio stream */
			NULL,                        /* no video stream */
			session->pool);              /* memory pool to allocate memory from */
	channel = mrcp_application_channel_create(
			session,                     /* session, channel belongs to */
			MRCP_SYNTHESIZER_RESOURCE,   /* MRCP resource identifier */
			termination,                 /* media termination, used to terminate audio stream */
			NULL,                        /* RTP descriptor, used to create RTP termination (NULL by default) */
			synth_channel);              /* object to associate */
	return channel;
}


/** Run demo synthesizer scenario */
static apt_bool_t synth_application_run(demo_application_t *demo_application, const char *profile)
{
	mrcp_channel_t *channel;
	/* create session */
	mrcp_session_t *session = mrcp_application_session_create(demo_application->application,profile,NULL);
	if(!session) {
		return FALSE;
	}
	
	/* create channel and associate all the required data */
	channel = synth_application_channel_create(session);
	if(!channel) {
		mrcp_application_session_destroy(session);
		return FALSE;
	}

	/* add channel to session (send asynchronous request) */
	if(mrcp_application_channel_add(session,channel) != TRUE) {
		/* session and channel are still not referenced 
		and both are allocated from session pool and will
		be freed with session destroy call */
		mrcp_application_session_destroy(session);
		return FALSE;
	}

	return TRUE;
}

/** Handle the messages sent from the MRCP client stack */
static apt_bool_t synth_application_handler(demo_application_t *application, const mrcp_app_message_t *app_message)
{
	/* app_message should be dispatched now,
	*  the default dispatcher is used in demo. */
	return mrcp_application_message_dispatch(&synth_application_dispatcher,app_message);
}

/** Handle the responses sent to session update requests */
static apt_bool_t synth_application_on_session_update(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* not used in demo */
	return TRUE;
}

/** Handle the responses sent to session terminate requests */
static apt_bool_t synth_application_on_session_terminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	/* received response to session termination request,
	now it's safe to destroy no more referenced session */
	mrcp_application_session_destroy(session);
	return TRUE;
}

/** Handle the responses sent to channel add requests */
static apt_bool_t synth_application_on_channel_add(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	synth_app_channel_t *synth_channel = mrcp_application_channel_object_get(channel);
	if(status == MRCP_SIG_STATUS_CODE_SUCCESS) {
		mrcp_message_t *mrcp_message;
		/* create and send SPEAK request */
		mrcp_message = demo_speak_message_create(session,channel);
		if(mrcp_message) {
			mrcp_application_message_send(session,channel,mrcp_message);
		}

		if(synth_channel && session) {
			const apt_dir_layout_t *dir_layout = mrcp_application_dir_layout_get(application);
			char *file_name = apr_pstrcat(session->pool,"synth-",session->id.buf,".pcm",NULL);
			char *file_path = apt_datadir_filepath_get(dir_layout,file_name,session->pool);
			if(file_path) {
				synth_channel->audio_out = fopen(file_path,"wb");
			}
		}
	}
	else {
		/* error case, just terminate the demo */
		mrcp_application_session_terminate(session);
	}
	return TRUE;
}

/** Handle the responses sent to channel remove requests */
static apt_bool_t synth_application_on_channel_remove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	synth_app_channel_t *synth_channel = mrcp_application_channel_object_get(channel);

	/* terminate the demo */
	mrcp_application_session_terminate(session);

	if(synth_channel) {
		FILE *audio_out = synth_channel->audio_out;
		if(audio_out) {
			synth_channel->audio_out = NULL;
			fclose(audio_out);
		}
	}
	return TRUE;
}

/** Handle the MRCP responses/events */
static apt_bool_t synth_application_on_message_receive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		/* received MRCP response */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK) {
			/* received the response to SPEAK request */
			if(message->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS) {
				/* waiting for SPEAK-COMPLETE event */
			}
			else {
				/* received unexpected response, remove channel */
				mrcp_application_channel_remove(session,channel);
			}
		}
		else {
			/* received unexpected response */
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		/* received MRCP event */
		if(message->start_line.method_id == SYNTHESIZER_SPEAK_COMPLETE) {
			/* received SPEAK-COMPLETE event, remove channel */
			mrcp_application_channel_remove(session,channel);
		}
	}
	return TRUE;
}

/** Callback is called from MPF engine context to destroy any additional data associated with audio stream */
static apt_bool_t synth_app_stream_destroy(mpf_audio_stream_t *stream)
{
	/* nothing to destroy in demo */
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action before open */
static apt_bool_t synth_app_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to perform application stream specific action after close */
static apt_bool_t synth_app_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

/** Callback is called from MPF engine context to make new frame available to write/send */
static apt_bool_t synth_app_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	synth_app_channel_t *synth_channel = stream->obj;
	if(synth_channel && synth_channel->audio_out) {
		fwrite(frame->codec_frame.buffer,1,frame->codec_frame.size,synth_channel->audio_out);
	}
	return TRUE;
}
