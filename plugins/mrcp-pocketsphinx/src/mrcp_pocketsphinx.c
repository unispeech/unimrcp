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

/** Methods of recognition stream  */
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
	pocketsphinx_engine_t *engine;
	/** Engine channel base */
	mrcp_engine_channel_t *channel;

	/** Actual recognizer object */
	ps_decoder_t          *decoder;
	/** Configuration */
	cmd_ln_t              *config;

	/** Thread to run recognition in */
	apr_thread_t          *thread;
	/** Conditional wait object */
	apr_thread_cond_t     *wait_object;
	/** Mutex of the wait object */
	apr_thread_mutex_t    *mutex;

	/** Pending request */
	mrcp_message_t        *request;

	apt_bool_t             recognizing;
};

/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

static void* APR_THREAD_FUNC pocketsphinx_recognizer_run(apr_thread_t *thread, void *data);

/** Create pocketsphinx engine (engine is an aggregation of recognizers) */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	pocketsphinx_engine_t *engine = apr_palloc(pool,sizeof(pocketsphinx_engine_t));
	
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
	recognizer->thread = NULL;
	recognizer->wait_object = NULL;
	recognizer->mutex = NULL;
	recognizer->request = NULL;
	recognizer->recognizing = FALSE;
	
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

/** Create pocketsphinx recognizer */
static apt_bool_t pocketsphinx_recognizer_destroy(mrcp_engine_channel_t *channel)
{
	return TRUE;
}

/** Open pocketsphinx recognizer (asynchronous response MUST be sent) */
static apt_bool_t pocketsphinx_recognizer_open(mrcp_engine_channel_t *channel)
{
	apr_status_t rv;
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	apr_thread_mutex_create(&recognizer->mutex,APR_THREAD_MUTEX_DEFAULT,channel->pool);
	apr_thread_cond_create(&recognizer->wait_object,channel->pool);

	/* Launch a thread to run recognition in */
	rv = apr_thread_create(&recognizer->thread,NULL,pocketsphinx_recognizer_run,recognizer,channel->pool);
	if(rv != APR_SUCCESS) {
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
	if(recognizer->thread) {
		apr_status_t rv;
		
		/* Signal recognition thread to terminate */
		recognizer->request = NULL;
		apr_thread_mutex_lock(recognizer->mutex);
		apr_thread_cond_signal(recognizer->wait_object);
		apr_thread_mutex_unlock(recognizer->mutex);

		apr_thread_join(&rv,recognizer->thread);
		recognizer->thread = NULL;
	}

	return mrcp_engine_channel_close_respond(channel);
}

/** Process MRCP request (asynchronous response MUST be sent)*/
static apt_bool_t pocketsphinx_recognizer_request_process(mrcp_engine_channel_t *channel, mrcp_message_t *request)
{
	pocketsphinx_recognizer_t *recognizer = channel->method_obj;

	/* Store request and signal recognition thread to process the request */
	recognizer->request = request;
	apr_thread_mutex_lock(recognizer->mutex);
	apr_thread_cond_signal(recognizer->wait_object);
	apr_thread_mutex_unlock(recognizer->mutex);
	return TRUE;
}





static apt_bool_t pocketsphinx_stream_write(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	pocketsphinx_recognizer_t *recognizer = stream->obj;

	if(recognizer->recognizing == TRUE) {
		int32 score;
		const char *utt_id;
		const char *hyp = ps_get_hyp(recognizer->decoder,&score,&utt_id);
		if(hyp) {
//			ps_end_utt(recognizer->decoder);
		}
	
		ps_process_raw(
			recognizer->decoder, 
			(const int16 *)frame->codec_frame.buffer, 
			frame->codec_frame.size / sizeof(int16),
			FALSE, 
			FALSE);
	}

	return TRUE;
}



/** Create pocketsphinx decoder */
static apt_bool_t pocketsphinx_decoder_create(pocketsphinx_recognizer_t *recognizer)
{
	mrcp_engine_channel_t *channel = recognizer->channel;
	const apt_dir_layout_t *dir_layout = channel->engine->dir_layout;
	const char *model = apt_datadir_filepath_get(dir_layout,"pocketsphinx/communicator",channel->pool);
	const char *grammar = apt_datadir_filepath_get(dir_layout,"pocketsphinx/demo.gram",channel->pool);
	const char *dictionary = apt_datadir_filepath_get(dir_layout,"pocketsphinx/default.dic",channel->pool);

	recognizer->config = cmd_ln_init(recognizer->config, ps_args(), FALSE,
							 "-samprate", "8000",
							 "-hmm", model,
							 "-jsgf", grammar,
							 "-dict", dictionary,
							 "-frate", "50",
							 "-silprob", "0.005",
							 NULL);

	if(!recognizer->config) {
		return FALSE;
	}
	
	recognizer->decoder = ps_init(recognizer->config);
	if(!recognizer->decoder) {
		return FALSE;
	}
	return TRUE;
}

/** Process RECOGNIZE request */
static apt_bool_t pocketsphinx_recognize(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	ps_start_utt(recognizer->decoder, NULL);
	recognizer->recognizing = TRUE;
	return TRUE;
}

/** Process STOP request */
static apt_bool_t pocketsphinx_stop(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request, mrcp_message_t *response)
{
	recognizer->recognizing = FALSE;
	ps_end_utt(recognizer->decoder);
	return TRUE;
}

/** Dispatch MRCP request */
static apt_bool_t pocketsphinx_request_dispatch(pocketsphinx_recognizer_t *recognizer, mrcp_message_t *request)
{
	apt_bool_t processed = FALSE;
	mrcp_message_t *response = mrcp_response_create(request,request->pool);
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
	mrcp_message_t *request;

	/** Create pocketsphinx decoder */
	apt_bool_t status = pocketsphinx_decoder_create(recognizer);
	/** Send response to channel_open request */
	mrcp_engine_channel_open_respond(recognizer->channel,status);

	do {
		/** Wait for MRCP requests */
		apr_thread_mutex_lock(recognizer->mutex);
		apr_thread_cond_wait(recognizer->wait_object,recognizer->mutex);
		apr_thread_mutex_unlock(recognizer->mutex);
		request = recognizer->request;
		recognizer->request = NULL;
		if(request) {
			pocketsphinx_request_dispatch(recognizer,request);
		}
	}
	while(request);

	/** Free pocketsphinx decoder */
	ps_free(recognizer->decoder);
	recognizer->decoder = NULL;

	/** Exit thread */
	apr_thread_exit(thread,APR_SUCCESS);
	return NULL;
}
