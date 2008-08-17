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

#include "apt_obj_list.h"
#include "mrcp_state_machine.h"
#include "mrcp_synth_state_machine.h"
#include "mrcp_synth_resource.h"
#include "mrcp_message.h"

/** MRCP synthesizer states */
typedef enum {
	SYNTHESIZER_STATE_IDLE,
	SYNTHESIZER_STATE_SPEAKING,
	SYNTHESIZER_STATE_PAUSED
} mrcp_synth_state_e;

typedef struct mrcp_synth_state_machine_t mrcp_synth_state_machine_t;
struct mrcp_synth_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t  base;
	/** synthesizer state */
	mrcp_synth_state_e    state;
	/** request sent to synthesizer engine and waiting for the response to be received */
	mrcp_message_t       *active_request;
	/** in-progress speak request */
	mrcp_message_t       *speaker;
	/** queue of pending speak requests */
	apt_obj_list_t       *queue;
	/** properties used in set(get) params */
	mrcp_message_header_t properties;
};

typedef apt_bool_t (*synth_method_f)(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t synth_message_dispatch(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return state_machine->base.dispatcher(&state_machine->base,message);
}


static apt_bool_t synth_request_set_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_get_params(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_speak(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_stop(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_pause(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_resume(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_barge_in_occurred(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_control(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_request_define_lexicon(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t synth_event_speech_marker(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->speaker) {
		/* unexpected event, no in-progress speak request */
		return FALSE;
	}

	if(state_machine->speaker->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	/* more to come */
	return synth_message_dispatch(state_machine,message);
}

static apt_bool_t synth_event_speak_complete(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->speaker) {
		/* unexpected event, no in-progress speak request */
		return FALSE;
	}

	if(state_machine->speaker->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	/* more to come */
	return synth_message_dispatch(state_machine,message);
}

static synth_method_f synth_request_method_array[SYNTHESIZER_METHOD_COUNT] = {
	synth_request_set_params,
	synth_request_get_params,
	synth_request_speak,
	synth_request_stop,
	synth_request_pause,
	synth_request_resume,
	synth_request_barge_in_occurred,
	synth_request_control,
	synth_request_define_lexicon
};

static synth_method_f synth_event_method_array[SYNTHESIZER_EVENT_COUNT] = {
	synth_event_speech_marker,
	synth_event_speak_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t synth_request_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	synth_method_f method;
	if(message->start_line.method_id >= SYNTHESIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	method = synth_request_method_array[message->start_line.method_id];
	return method(state_machine,message);
}

/** Update state according to received outgoing response from synthesizer engine */
static apt_bool_t synth_response_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->active_request) {
		/* unexpected response, no active request waiting for response */
		return FALSE;
	}
	if(state_machine->active_request->start_line.request_id != message->start_line.request_id) {
		/* unexpected response, request id doesn't match */
		return FALSE;
	}

	state_machine->active_request = NULL;
	return synth_message_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from synthesizer engine */
static apt_bool_t synth_event_state_update(mrcp_synth_state_machine_t *state_machine, mrcp_message_t *message)
{
	synth_method_f method;
	if(message->start_line.method_id >= SYNTHESIZER_EVENT_COUNT) {
		return FALSE;
	}
	
	method = synth_event_method_array[message->start_line.method_id];
	return method(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from synthesizer engine */
static apt_bool_t synth_state_update(mrcp_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_synth_state_machine_t *synth_state_machine = (mrcp_synth_state_machine_t*)state_machine;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = synth_request_state_update(synth_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = synth_response_state_update(synth_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = synth_event_state_update(synth_state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Create MRCP synthesizer server state machine */
mrcp_state_machine_t* mrcp_synth_server_state_machine_create(void *obj, mrcp_message_dispatcher_f dispatcher, mrcp_version_e version, apr_pool_t *pool)
{
	mrcp_synth_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_synth_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj,dispatcher);
	state_machine->base.update = synth_state_update;
	state_machine->state = SYNTHESIZER_STATE_IDLE;
	state_machine->active_request = NULL;
	state_machine->speaker = NULL;
	state_machine->queue = apt_list_create(pool);
	mrcp_message_header_init(&state_machine->properties);
	return &state_machine->base;
}
