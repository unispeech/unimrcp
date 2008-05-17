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

#ifndef __MRCP_RECOG_HEADER_H__
#define __MRCP_RECOG_HEADER_H__

/**
 * @file mrcp_recog_header.h
 * @brief MRCP Recognizer Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP recognizer headers */
typedef enum {
	RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD,
	RECOGNIZER_HEADER_SENSITIVITY_LEVEL,
	RECOGNIZER_HEADER_SPEED_VS_ACCURACY,
	RECOGNIZER_HEADER_N_BEST_LIST_LENGTH,
	RECOGNIZER_HEADER_NO_INPUT_TIMEOUT,
	RECOGNIZER_HEADER_RECOGNITION_TIMEOUT,
	RECOGNIZER_HEADER_WAVEFORM_URI,
	RECOGNIZER_HEADER_INPUT_WAVEFORM_URI,
	RECOGNIZER_HEADER_COMPLETION_CAUSE,
	RECOGNIZER_HEADER_RECOGNIZER_CONTEXT_BLOCK,
	RECOGNIZER_HEADER_START_INPUT_TIMERS,
	RECOGNIZER_HEADER_VENDOR_SPECIFIC,
	RECOGNIZER_HEADER_SPEECH_COMPLETE_TIMEOUT,
	RECOGNIZER_HEADER_SPEECH_INCOMPLETE_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_INTERDIGIT_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_TERM_TIMEOUT,
	RECOGNIZER_HEADER_DTMF_TERM_CHAR,
	RECOGNIZER_HEADER_FETCH_TIMEOUT,
	RECOGNIZER_HEADER_FAILED_URI,
	RECOGNIZER_HEADER_FAILED_URI_CAUSE,
	RECOGNIZER_HEADER_SAVE_WAVEFORM,
	RECOGNIZER_HEADER_NEW_AUDIO_CHANNEL,
	RECOGNIZER_HEADER_SPEECH_LANGUAGE,
	RECOGNIZER_HEADER_VER_BUFFER_UTTERANCE,
	RECOGNIZER_HEADER_RECOGNITION_MODE,
	RECOGNIZER_HEADER_CANCEL_IF_QUEUE,
	RECOGNIZER_HEADER_HOTWORD_MAX_DURATION,
	RECOGNIZER_HEADER_HOTWORD_MIN_DURATION,
	RECOGNIZER_HEADER_DTMF_BUFFER_TIME,
	RECOGNIZER_HEADER_CLEAR_DTMF_BUFFER,
	RECOGNIZER_HEADER_EARLY_NO_MATCH,

	RECOGNIZER_HEADER_COUNT
} mrcp_recognizer_header_id;


/** MRCP recognizer completion-cause  */
typedef enum {
	RECOGNIZER_COMPLETION_CAUSE_SUCCESS                 = 0,
	RECOGNIZER_COMPLETION_CAUSE_NO_MATCH                = 1,
	RECOGNIZER_COMPLETION_CAUSE_NO_INPUT_TIMEOUT        = 2,
	RECOGNIZER_COMPLETION_CAUSE_RECOGNITION_TIMEOUT     = 3,
	RECOGNIZER_COMPLETION_CAUSE_GRAM_LOAD_FAILURE       = 4,
	RECOGNIZER_COMPLETION_CAUSE_GRAM_COMP_FAILURE       = 5,
	RECOGNIZER_COMPLETION_CAUSE_ERROR                   = 6,
	RECOGNIZER_COMPLETION_CAUSE_SPEECH_TOO_EARLY        = 7,
	RECOGNIZER_COMPLETION_CAUSE_TOO_MUCH_SPEECH_TIMEOUT = 8,
	RECOGNIZER_COMPLETION_CAUSE_URI_FAILURE             = 9,
	RECOGNIZER_COMPLETION_CAUSE_LANGUAGE_UNSUPPORTED    = 10,
	RECOGNIZER_COMPLETION_CAUSE_CANCELLED               = 11,
	RECOGNIZER_COMPLETION_CAUSE_SEMANTICS_FAILURE       = 12,
	
	RECOGNIZER_COMPLETION_CAUSE_COUNT                   = 13,
	RECOGNIZER_COMPLETION_CAUSE_UNKNOWN                 = RECOGNIZER_COMPLETION_CAUSE_COUNT
} mrcp_recog_completion_cause_e;



typedef struct mrcp_recog_header_t mrcp_recog_header_t;

/** MRCP recognizer-header */
struct mrcp_recog_header_t {
	float                         confidence_threshold;
	apr_size_t                    sensitivity_level;
	apr_size_t                    speed_vs_accuracy;
	apr_size_t                    n_best_list_length;
	apr_size_t                    no_input_timeout;
	apr_size_t                    recognition_timeout;
	apt_str_t                     waveform_uri;
	apt_str_t                     input_waveform_uri;
	mrcp_recog_completion_cause_e completion_cause;
	apt_str_t                     recognizer_context_block;
	apt_bool_t                    start_input_timers;
	apt_str_t                     vendor_specific;
	apr_size_t                    speech_complete_timeout;
	apr_size_t                    speech_incomplete_timeout;
	apr_size_t                    dtmf_interdigit_timeout;
	apr_size_t                    dtmf_term_timeout;
	char                          dtmf_term_char;
	apr_size_t                    fetch_timeout;
	apt_str_t                     failed_uri;
	apt_str_t                     failed_uri_cause;
	apt_bool_t                    save_waveform;
	apt_bool_t                    new_audio_channel;
	apt_str_t                     speech_language;
	apt_bool_t                    ver_buffer_utterance;
	apt_str_t                     recognition_mode;
	apt_bool_t                    cancel_if_queue;
	apr_size_t                    hotword_max_duration;
	apr_size_t                    hotword_min_duration;
	apr_size_t                    dtmf_buffer_time;
	apt_bool_t                    clear_dtmf_buffer;
	apt_bool_t                    early_no_match;
};


/** Get recognizer header vtable */
MRCP_DECLARE(const mrcp_header_vtable_t*) mrcp_recog_header_vtable_get(mrcp_version_e version);


APT_END_EXTERN_C

#endif /*__MRCP_RECOG_HEADER_H__*/
