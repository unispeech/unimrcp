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

/* 
 * Some mandatory rules for plugin implementation.
 * 1. Each plugin MUST contain the following function as an entry point of the plugin
 *        MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
 * 2. One and only one response MUST be sent back to the received request.
 * 3. Methods (callbacks) of the MRCP engine channel MUST not block.
 *   (asynch response can be sent from the context of other thread)
 * 4. Methods (callbacks) of the MPF engine stream MUST not block.
 */

#include <pocketsphinx.h>
#include <apr_thread_cond.h>
#include <apr_thread_proc.h>
#include "mrcp_resource_engine.h"
#include "mrcp_recog_resource.h"
#include "mrcp_recog_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "mpf_activity_detector.h"
#include "apt_log.h"


#define RECOGNIZER_SIDRES(recognizer) \
	(recognizer)->channel->id.buf ? (recognizer)->channel->id.buf : "new", "pocketsphinx"

typedef struct pocketsphinx_engine_t pocketsphinx_engine_t;
typedef struct pocketsphinx_recognizer_t pocketsphinx_recognizer_t;

/** Methods of recognition engine */
static apt_bool_t pocketsphinx_engine_destroy(mrcp_resource_engine_t *engine);
static apt_bool_t pocketsphinx_engine_open(mrcp_resource_engine_t *engine);
static apt_bool_t pocketsphinx_engine_close(mrcp_resource_engine_t *engine);
static mrcp_engine_channel_t* pocketsphinx_engine_recognizer_create(mrcp_resource_engine_t *engine, apr_pool_t *pool);

static const struct mrcp_engine_method_vtable_t engine_vtable = {
	pocketsphinx_engine_destroy,
	pocketsphinx_engine_open,
	pocketsphinx_engine_close,
	pocketsphinx_engine_recognizer_create
};


/** Methods of recognition channel (recognizer) */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel);
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request);

static const struct mrcp_engine_channel_method_vtable_t channel_vtable = {
	pocketsphinx_recognizer_destroy,
	pocketsphinx_recognizer_open,
	pocketsphinx_recognizer_close,
	pocketsphinx_recognizer_request_process
};

/** Methods of audio stream to recognize  */
static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame);

static const mpf_audio_stream_vtable_t audio_stream_vtable = {
	NULL, /* destroy */
	NULL, /* open_rx */
	NULL, /* close_rx */
	NULL, /* read_frame */
	NULL, /* open_tx */
	NULL, /* close_tx */
	pocketsphinx_stream_write
};

/** Declaration of pocketsphinx engine (engine is an aggregation of recognizers) */
struct pocketsphinx_engine_t {
	mrcp_resource_engine_t *base;
};

/** Declaration of pocketsphinx channel (recognizer) */
struct pocketsphinx_recognizer_t {
	/** Back pointer to engine */
	pocketsphinx_engine_t   *engine;
	/** Engine channel base */
	mrcp_engine_channel_t   *channel;

	/** Actual recognizer object */
	ps_decoder_t            *decoder;
	/** Configuration */
	cmd_ln_t                *config;
	/** Timeout elapsed since the last partial result checking */
	int                      partial_result_timeout;
	/** Last (partially) recognized result */
	const char              *last_result;

	/** Voice activity detector */
	mpf_activity_detector_t *detector;

	/** Thread to run recognition in */
	apr_thread_t            *thread;
	/** Conditional wait object */
	apr_thread_cond_t       *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t      *mutex;

	/** Pending request from client stack to recognizer */
	mrcp_message_t          *request;
	/** Pending event from mpf layer to recognizer */
	mrcp_message_t          *complete_event;
	/** In-progress RECOGNIZE request */
	mrcp_message_t          *inprogress_recog;
	/** Pending STOP response */
	mrcp_message_t          *stop_response;
	/** Is recognition channel being closed */
	apt_bool_t               close_requested;
};

/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

static void* APR_THREAD_FUNC pocketsphinx_recognizer_run(apr_thread_t *thread, void *data);

/** Create pocketsphinx engine (engine is an aggregation of recognizers) */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	pocketsphinx_engine_t *engine = apr_palloc(pool,sizeof(pocketsphinx_engine_t));
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Create PocketSphinx Engine");
	
	/* create resource engine base */
	engine->base = mrcp_resource_engine_create(
					MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
					engine,                    /* object to associate */
					&engine_vtable,            /* virtual methods table of resource engine */
					pool);                     /* pool to allocate memory from */
	return engine->base;
}

/** Destroy pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_destroy(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

/** Open pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_open(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

/** Close pocketsphinx engine */
static apt_bool_t pocketsphinx_engine_close(mrcp_resource_engine_t *engine)
{
	return TRUE;
}

/** Create pocketsphinx recognizer */
static mrcp_engine_channel_t* pocketsphinx_engine_recognizer_create(mrcp_resource_engine_t *engine, apr_pool_t *pool)
{
	mrcp_engine_channel_t *channel;
	pocketsphinx_recognizer_t *recognizer = apr_palloc(pool,sizeof(pocketsphinx_recognizer_t));
//	recognizer->engine = engine;
	recognizer->decoder = NULL;
	recognizer->config = NULL;
	recognizer->partial_result_timeout = 0;
	recognizer->last_result = NULL;
	recognizer->detector = NULL;
	recognizer->thread = NULL;
	recognizer->wait_object = NULL;
	recognizer->mutex = NULL;
	recognizer->request = NULL;
	recognizer->complete_event = NULL;
	recognizer->inprogress_recog = FALSE;
	recognizer->stop_response = NULL;
	recognizer->close_requested = FALSE;
	
	/* create engine channel base */
	channel = mrcp_engine_sink_channel_create(
			engine,               /* resource engine */
			&channel_vtable,      /* virtual methods table of engine channel */
			&audio_stream_vtable, /* virtual methods table of audio stream */
			recognizer,           /* object to associate */
			NULL,                 /* codec descriptor might be NULL by default */
			pool);                /* pool to allocate memory from */
	
	recognizer->channel = channel;
	return channel;
}

/** Destroy pocketsphinx recognizer */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel)
{
	return TRUE;
}

/** Open pocketsphinx recognizer (asynchronous response MUST be sent) */
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel)
{
	apr_status_t rv;
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open Channel "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));

	apr_thread_mutex_create(&recognizer->mutex,APR_THREAD_MUTEX_DEFAULT,channel->pool);
	apr_thread_cond_create(&recognizer->wait_object,channel->pool);

	/* Launch a thread to run recognition in */
	rv = apr_thread_create(&recognizer->thread,NULL,pocketsphinx_recognizer_run,recognizer,channel->pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Failed to Launch Thread "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		apr_thread_mutex_destroy(recognizer->mutex);
		recognizer->mutex = NULL;
		apr_thread_cond_destroy(recognizer->wait_object);
		recognizer->wait_object = NULL;
		return mrcp_engine_channel_open_respond(channel,FALSE);
	}

	return TRUE;
}

/** Close pocketsphinx recognizer (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_close(mrcp_engine_channel_t *channel)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Close Channel "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
	if(recognizer->thread) {
		apr_status_t rv;
		
		/* Signal recognition thread to terminate */
		apr_thread_mutex_lock(recognizer->mutex);
		recognizer->close_requested = TRUE;
		apr_thread_cond_signal(recognizer->wait_object);
		apr_thread_mutex_unlock(recognizer->mutex);

		apr_thread_join(&rv,recognizer->thread);
		recognizer->thread = NULL;

		apr_thread_mutex_destroy(recognizer->mutex);
		recognizer->mutex = NULL;
		apr_thread_cond_destroy(recognizer->wait_object);
		recognizer->wait_object = NULL;
	}

	return mrcp_engine_channel_close_respond(channel);
}

/** Process MRCP request (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	/* Store request and signal recognition thread to process the request */
	apr_thread_mutex_lock(recognizer->mutex);
	recognizer->request = request;
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}




/* Start of input (utterance) [MPF]  */
static apt_bool_t pocketsphinx_start_of_input(pocketsphinx_recognizer_t *recognizer)
{
	/* create START-OF-INPUT event */
	mrcp_message_t *message = mrcp_event_create(
						recognizer->inprogress_recog,
						RECOGNIZER_START_OF_INPUT,
						recognizer->inprogress_recog->pool);
	if(!message) {
		return FALSE;
	}

	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynch event */
	return mrcp_engine_channel_message_send(recognizer->channel,message);
}

/* End of input (utterance) [MPF] */
static apt_bool_t pocketsphinx_end_of_input(pocketsphinx_recognizer_t *recognizer, mrcp_recog_completion_cause_e cause)
{
	mrcp_recog_header_t *recog_header;
	/* create RECOGNITION-COMPLETE event */
	mrcp_message_t *message = mrcp_event_create(
						recognizer->inprogress_recog,
						RECOGNIZER_RECOGNITION_COMPLETE,
						recognizer->inprogress_recog->pool);
	if(!message) {
		return FALSE;
	}

	/* get/allocate recognizer header */
	recog_header = mrcp_resource_header_prepare(message);
	if(recog_header) {
		/* set completion cause */
		recog_header->completion_cause = cause;
		mrcp_resource_header_property_add(message,RECOGNIZER_HEADER_COMPLETION_CAUSE);
	}
	/* set request state */
	message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;

	/* signal recognition thread first */
	apr_thread_mutex_lock(recognizer->mutex);
	recognizer->complete_event = message;
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}

/* Process audio frame [MPF] */
static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	pocketsphinx_recognizer_t *recognizer = stream->obj;

	/* check whether recognition has been started and not completed yet */
	if(recognizer->inprogress_recog && !recognizer->complete_event) {
		mpf_detector_event_e det_event;

		/* first check if STOP has been requested */
		if(recognizer->stop_response) {
			/* recognition has been stopped -> acknowledge with complete-event */
			pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_SUCCESS);
			return TRUE;
		}

		if(ps_process_raw(
					recognizer->decoder, 
					(const int16 *)frame->codec_frame.buffer, 
					frame->codec_frame.size / sizeof(int16),
					FALSE, 
					FALSE) < 0) {

			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Process Raw Data "APT_SIDRES_FMT,
				RECOGNIZER_SIDRES(recognizer));
		}

		recognizer->partial_result_timeout += CODEC_FRAME_TIME_BASE;
		if(recognizer->partial_result_timeout == 100) {
			int32 score;
			char const *hyp;
			char const *uttid;

			recognizer->partial_result_timeout = 0;
			hyp = ps_get_hyp(recognizer->decoder, &score, &uttid);
			if(hyp && strlen(hyp) > 0) {
				if(recognizer->last_result == NULL || 0 != strcmp(recognizer->last_result, hyp)) {
					recognizer->last_result = apr_pstrdup(recognizer->channel->pool,hyp);
					apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Recognition Partial Result [%s] Score [%d] "APT_SIDRES_FMT,
						hyp,score,RECOGNIZER_SIDRES(recognizer));
				}
			}
		}
		
		det_event = mpf_activity_detector_process(recognizer->detector,frame);
		switch(det_event) {
			case MPF_DETECTOR_EVENT_ACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Activity "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_start_of_input(recognizer);
				break;
			case MPF_DETECTOR_EVENT_INACTIVITY:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Voice Inactivity "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_SUCCESS);
				break;
			case MPF_DETECTOR_EVENT_NOINPUT:
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Detected Noinput "APT_SIDRES_FMT,
					RECOGNIZER_SIDRES(recognizer));
				pocketsphinx_end_of_input(recognizer,RECOGNIZER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT);
				break;
			default:
				break;
		}
	}

	return TRUE;
}



/** Create pocketsphinx decoder [RECOG] */
static apt_bool_t pocketsphinx_decoder_create(pocketsphinx_recognizer_t *recognizer)
{
	mrcp_engine_channel_t *channel = recognizer->channel;
	const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
	const char *model = apt_datadir_filepath_get(dir_layout,"pocketsphinx/communicator",channel->pool);
	const char *grammar = apt_datadir_filepath_get(dir_layout,"pocketsphinx/demo.gram",channel->pool);
	const char *dictionary = apt_datadir_filepath_get(dir_layout,"pocketsphinx/default.dic",channel->pool);

	apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Init Config "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
	recognizer->config = cmd_ln_init(recognizer->config, ps_args(), FALSE,
							 "-samprate", "8000",
							 "-hmm", model,
							 "-jsgf", grammar,
							 "-dict", dictionary,
							 "-frate", "50",
							 "-silprob", "0.005",
							 NULL);

	if(!recognizer->config) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Init Config "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		return FALSE;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Init Decoder "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
	recognizer->decoder = ps_init(recognizer->config);
	if(!recognizer->decoder) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Init Decoder "APT_SIDRES_FMT,RECOGNIZER_SIDRES(recognizer));
		return FALSE;
	}

	recognizer->detector = mpf_activity_detector_create(channel->pool);
	return TRUE;
}

/** Process RECOGNIZE request [RECOG] */
static apt_bool_t pocketsphinx_recognize(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	if(ps_start_utt(recognizer->decoder, NULL) < 0) {
		response->start_line.status_code = MRCP_STATUS_CODE_METHOD_FAILED;
		return FALSE;
	}
	
	response->start_line.request_state = MRCP_REQUEST_STATE_INPROGRESS;
	/* send asynchronous response */
	mrcp_engine_channel_message_send(recognizer->channel,response);

	/* reset */
	mpf_activity_detector_reset(recognizer->detector);
	recognizer->last_result = NULL;
	recognizer->partial_result_timeout = 0;
	recognizer->complete_event = NULL;
	recognizer->inprogress_recog = request;
	return TRUE;
}

/** Process STOP request [RECOG] */
static apt_bool_t pocketsphinx_stop(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	if(recognizer->inprogress_recog) {
		/* store pending STOP response for further processing */
		recognizer->stop_response = response;
		return TRUE;
	}

	/* send asynchronous response */
	mrcp_engine_channel_message_send(recognizer->channel,response);
	return TRUE;
}

/** Process RECOGNITION-COMPLETE event [RECOG] */
static apt_bool_t pocketsphinx_recognition_complete(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *complete_event)
{
	mrcp_recog_header_t *recog_header;
	if(!recognizer->inprogress_recog) {
		/* false event */
		return FALSE;
	}

	recognizer->inprogress_recog = NULL;
	ps_end_utt(recognizer->decoder);

	if(recognizer->stop_response) {
		/* recognition has been stopped, send STOP response instead */
		mrcp_message_t *response = recognizer->stop_response;
		recognizer->stop_response = NULL;
		if(recognizer->close_requested == FALSE) {
			mrcp_engine_channel_message_send(recognizer->channel,response);
		}
		return TRUE;
	}
	
	recog_header = mrcp_resource_header_get(complete_event);
	if(recog_header->completion_cause == RECOGNIZER_COMPLETION_CAUSE_SUCCESS) {
		int32 score;
		char const *hyp;
		char const *uttid;

		hyp = ps_get_hyp(recognizer->decoder, &score, &uttid);
		if(hyp && strlen(hyp) > 0) {
			apt_str_t *body = &complete_event->body;
			recognizer->last_result = apr_pstrdup(recognizer->channel->pool,hyp);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Get Recognition Final Result [%s] Score [%d] "APT_SIDRES_FMT,
				hyp,score,RECOGNIZER_SIDRES(recognizer));

			body->buf = apr_psprintf(complete_event->pool,
				"<interpretation grammar=\"%s\" score=\"%d\">\n"
				"  <result name=\"%s\">%s</result>\n"
				"  <input>%s</input>\n"
				"</interpretation>",
				"demo-id",87,//recognizer->grammar, recognizer->confidence,
				"match",
				recognizer->last_result,
				recognizer->last_result);
			if(body->buf) {
				mrcp_generic_header_t *generic_header;
				generic_header = mrcp_generic_header_prepare(complete_event);
				if(generic_header) {
					/* set content type */
					apt_string_assign(&generic_header->content_type,"application/x-nlsml",complete_event->pool);
					mrcp_generic_header_property_add(complete_event,GENERIC_HEADER_CONTENT_TYPE);
				}
				
				body->length = strlen(body->buf);
			}
		}
		else {
			recog_header->completion_cause = RECOGNIZER_COMPLETION_CAUSE_NO_MATCH;
		}
	}

	/* send asynchronous event */
	mrcp_engine_channel_message_send(recognizer->channel,complete_event);
	return TRUE;
}

/** Dispatch MRCP request [RECOG] */
static apt_bool_t pocketsphinx_request_dispatch(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request)
{
	apt_bool_t processed = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Dispatch Request %s "APT_SIDRES_FMT,
		request->start_line.method_name.buf,
		RECOGNIZER_SIDRES(recognizer));
	switch(request->start_line.method_id) {
		case RECOGNIZER_SET_PARAMS:
			break;
		case RECOGNIZER_GET_PARAMS:
			break;
		case RECOGNIZER_DEFINE_GRAMMAR:
			break;
		case RECOGNIZER_RECOGNIZE:
			processed = pocketsphinx_recognize(recognizer,request,response);
			break;
		case RECOGNIZER_GET_RESULT:
			break;
		case RECOGNIZER_START_INPUT_TIMERS:
			break;
		case RECOGNIZER_STOP:
			processed = pocketsphinx_stop(recognizer,request,response);
			break;
		default:
			break;
	}
	if(processed == FALSE) {
		/* send asynchronous response for non handled request */
		mrcp_engine_channel_message_send(recognizer->channel,response);
	}
	return TRUE;
}


/** Recognition thread */
static void* APR_THREAD_FUNC pocketsphinx_recognizer_run(apr_thread_t *thread, void *data)
{
	pocketsphinx_recognizer_t *recognizer = data;
	apt_bool_t status;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Run Recognition Thread "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
	/** Create pocketsphinx decoder */
	status = pocketsphinx_decoder_create(recognizer);
	/** Send response to channel_open request */
	mrcp_engine_channel_open_respond(recognizer->channel,status);

	do {
		/** Wait for MRCP requests */
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Wait for incoming messages "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
		apr_thread_mutex_lock(recognizer->mutex);
		apr_thread_cond_wait(recognizer->wait_object,recognizer->mutex);
		apr_thread_mutex_unlock(recognizer->mutex);

		if(recognizer->request) {
			/* store request message and further dispatch it */
			mrcp_message_t *request = recognizer->request;
			recognizer->request = NULL;
			pocketsphinx_request_dispatch(recognizer,request);
		}
		if(recognizer->complete_event) {
			/* end of input detected, get recognition result and raise recognition complete event */
			pocketsphinx_recognition_complete(recognizer,recognizer->complete_event);
		}
	}
	while(recognizer->close_requested == FALSE);

	/* check if recognition is still active */
	if(recognizer->inprogress_recog) {
		apr_thread_mutex_lock(recognizer->mutex);
		recognizer->stop_response = recognizer->inprogress_recog;
		apr_thread_cond_wait(recognizer->wait_object,recognizer->mutex);
		apr_thread_mutex_unlock(recognizer->mutex);
		if(recognizer->complete_event) {
			pocketsphinx_recognition_complete(recognizer,recognizer->complete_event);
		}
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Free Decoder "APT_SIDRES_FMT, RECOGNIZER_SIDRES(recognizer));
	/** Free pocketsphinx decoder */
	ps_free(recognizer->decoder);
	recognizer->decoder = NULL;

	/** Exit thread */
	apr_thread_exit(thread,APR_SUCCESS);
	return NULL;
}
