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

#ifndef __MRCP_SYNTH_HEADER_H__
#define __MRCP_SYNTH_HEADER_H__

/**
 * @file mrcp_synth_header.h
 * @brief MRCP Synthesizer Header
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP synthesizer headers */
typedef enum {
	SYNTHESIZER_HEADER_JUMP_SIZE,
	SYNTHESIZER_HEADER_KILL_ON_BARGE_IN,
	SYNTHESIZER_HEADER_SPEAKER_PROFILE,
	SYNTHESIZER_HEADER_COMPLETION_CAUSE,
	SYNTHESIZER_HEADER_COMPLETION_REASON,
	SYNTHESIZER_HEADER_VOICE_GENDER,
	SYNTHESIZER_HEADER_VOICE_AGE,
	SYNTHESIZER_HEADER_VOICE_VARIANT,
	SYNTHESIZER_HEADER_VOICE_NAME,
	SYNTHESIZER_HEADER_PROSODY_VOLUME,
	SYNTHESIZER_HEADER_PROSODY_RATE,
	SYNTHESIZER_HEADER_SPEECH_MARKER,
	SYNTHESIZER_HEADER_SPEECH_LANGUAGE,
	SYNTHESIZER_HEADER_FETCH_HINT,
	SYNTHESIZER_HEADER_FETCH_TIMEOUT,
	SYNTHESIZER_HEADER_AUDIO_FETCH_HINT,
	SYNTHESIZER_HEADER_FAILED_URI,
	SYNTHESIZER_HEADER_FAILED_URI_CAUSE,
	SYNTHESIZER_HEADER_SPEAK_RESTART,
	SYNTHESIZER_HEADER_SPEAK_LENGTH,
	SYNTHESIZER_HEADER_LOAD_LEXICON,
	SYNTHESIZER_HEADER_LEXICON_SEARCH_ORDER,

	SYNTHESIZER_HEADER_COUNT
} mrcp_synthesizer_header_id;


/** Speech-units */
typedef enum {
	SPEECH_UNIT_SECOND,
	SPEECH_UNIT_WORD,
	SPEECH_UNIT_SENTENCE,
	SPEECH_UNIT_PARAGRAPH,

	SPEECH_UNIT_COUNT
} mrcp_speech_unit_e;

/** Speech-length types */
typedef enum {
	SPEECH_LENGTH_TYPE_TEXT,
	SPEECH_LENGTH_TYPE_NUMERIC_POSITIVE,
	SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE,

	SPEECH_LENGTH_TYPE_UNKNOWN
} mrcp_speech_length_type_e;

/** MRCP voice-gender */
typedef enum {
	VOICE_GENDER_MALE,
	VOICE_GENDER_FEMALE,
	VOICE_GENDER_NEUTRAL,
	
	VOICE_GENDER_COUNT,
	VOICE_GENDER_UNKNOWN = VOICE_GENDER_COUNT
} mrcp_voice_gender_e;

/** Prosody-volume */
typedef enum {
	PROSODY_VOLUME_SILENT,
	PROSODY_VOLUME_XSOFT,
	PROSODY_VOLUME_SOFT,
	PROSODY_VOLUME_MEDIUM,
	PROSODY_VOLUME_LOUD,
	PROSODY_VOLUME_XLOUD,
	PROSODY_VOLUME_DEFAULT,

	PROSODY_VOLUME_COUNT,
	PROSODY_VOLUME_UNKNOWN = PROSODY_VOLUME_COUNT
} mrcp_prosody_volume_e;

/** Prosody-rate */
typedef enum {
	PROSODY_RATE_XSLOW,
	PROSODY_RATE_SLOW,
	PROSODY_RATE_MEDIUM,
	PROSODY_RATE_FAST,
	PROSODY_RATE_XFAST,
	PROSODY_RATE_DEFAULT,

	PROSODY_RATE_COUNT,
	PROSODY_RATE_UNKNOWN = PROSODY_RATE_COUNT
} mrcp_prosody_rate_e;

/** Synthesizer completion-cause specified in SPEAK-COMPLETE event */
typedef enum {
	SYNTHESIZER_COMPLETION_CAUSE_NORMAL               = 0,
	SYNTHESIZER_COMPLETION_CAUSE_BARGE_IN             = 1,
	SYNTHESIZER_COMPLETION_CAUSE_PARSE_FAILURE        = 2,
	SYNTHESIZER_COMPLETION_CAUSE_URI_FAILURE          = 3,
	SYNTHESIZER_COMPLETION_CAUSE_ERROR                = 4,
	SYNTHESIZER_COMPLETION_CAUSE_LANGUAGE_UNSUPPORTED = 5,
	SYNTHESIZER_COMPLETION_CAUSE_LEXICON_LOAD_FAILURE = 7,
	SYNTHESIZER_COMPLETION_CAUSE_CANCELLED            = 8,

	SYNTHESIZER_COMPLETION_CAUSE_COUNT                = 9,
	SYNTHESIZER_COMPLETION_CAUSE_UNKNOWN              = SYNTHESIZER_COMPLETION_CAUSE_COUNT
} mrcp_synth_completion_cause_e;



typedef struct mrcp_speech_length_value_t mrcp_speech_length_value_t;
typedef struct mrcp_numeric_speech_length_t mrcp_numeric_speech_length_t;
typedef struct mrcp_prosody_param_t mrcp_prosody_param_t;
typedef struct mrcp_voice_param_t mrcp_voice_param_t;
typedef struct mrcp_synth_header_t mrcp_synth_header_t;

/** Numeric speech-length */
struct mrcp_numeric_speech_length_t {
	apr_size_t         length;
	mrcp_speech_unit_e unit;
};

/** Definition of speech-length value */
struct mrcp_speech_length_value_t {
	mrcp_speech_length_type_e type;
	union {
		apt_str_t                    tag;
		mrcp_numeric_speech_length_t numeric;
	} value;
};

/** MRCP voice-param */
struct mrcp_voice_param_t {
	mrcp_voice_gender_e gender;
	apr_size_t          age;
	apr_size_t          variant;
	apt_str_t           name;
};

/** MRCP prosody-param */
struct mrcp_prosody_param_t {
	mrcp_prosody_volume_e volume;
	mrcp_prosody_rate_e   rate;
};

/** MRCP synthesizer-header */
struct mrcp_synth_header_t {
	mrcp_speech_length_value_t    jump_size;
	apt_bool_t                    kill_on_barge_in;
	apt_str_t                     speaker_profile;
	mrcp_synth_completion_cause_e completion_cause;
	apt_str_t                     completion_reason;
	mrcp_voice_param_t            voice_param;
	mrcp_prosody_param_t          prosody_param;
	apt_str_t                     speech_marker;
	apt_str_t                     speech_language;
	apt_str_t                     fetch_hint;
	apt_str_t                     audio_fetch_hint;
	apr_size_t                    fetch_timeout;
	apt_str_t                     failed_uri;
	apt_str_t                     failed_uri_cause;
	apt_bool_t                    speak_restart;
	mrcp_speech_length_value_t    speak_length;
	apt_bool_t                    load_lexicon;
	apt_str_t                     lexicon_search_order;
};

/** Get synthesizer header vtable */
MRCP_DECLARE(const mrcp_header_vtable_t*) mrcp_synth_header_vtable_get(mrcp_version_e version);


APT_END_EXTERN_C

#endif /*__MRCP_SYNTH_HEADER_H__*/
