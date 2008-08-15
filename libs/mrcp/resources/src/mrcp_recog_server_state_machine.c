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
#include "mrcp_recog_state_machine.h"
#include "mrcp_recog_resource.h"
#include "mrcp_message.h"

/** MRCP recognizer states */
typedef enum {
	RECOGNIZER_STATE_IDLE,
	RECOGNIZER_STATE_RECOGNIZING,
	RECOGNIZER_STATE_RECOGNIZED
} mrcp_recog_state_e;

typedef struct mrcp_recog_state_machine_t mrcp_recog_state_machine_t;
struct mrcp_recog_state_machine_t {
	/** state machine base */
	mrcp_state_machine_t  base;
	/** recognizer state */
	mrcp_recog_state_e    state;
	/** request sent to recognition engine and waiting for the response to be received */
	mrcp_message_t       *active_request;
	/** in-progress recognize request */
	mrcp_message_t       *recog;
	/** queue of pending recognition requests */
	apt_obj_list_t       *queue;
	/** properties used in set(get) params */
	mrcp_message_header_t properties;
};

typedef apt_bool_t (*recog_method_f)(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message);

static APR_INLINE apt_bool_t recog_message_dispatch(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return state_machine->base.dispatcher(&state_machine->base,message);
}


static apt_bool_t recog_request_set_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_get_params(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_define_grammar(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_recognize(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_get_result(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_recognition_start_timers(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_request_stop(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	return TRUE;
}

static apt_bool_t recog_event_start_of_speech(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->recog) {
		/* unexpected event, no in-progress recognition request */
		return FALSE;
	}

	if(state_machine->recog->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	/* more to come */
	return recog_message_dispatch(state_machine,message);
}

static apt_bool_t recog_event_recognition_complete(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	if(!state_machine->recog) {
		/* unexpected event, no in-progress recognition request */
		return FALSE;
	}

	if(state_machine->recog->start_line.request_id != message->start_line.request_id) {
		/* unexpected event */
		return FALSE;
	}
	
	/* more to come */
	return recog_message_dispatch(state_machine,message);
}

static recog_method_f recog_request_method_array[RECOGNIZER_METHOD_COUNT] = {
	recog_request_set_params,
	recog_request_get_params,
	recog_request_define_grammar,
	recog_request_recognize,
	recog_request_get_result,
	recog_request_recognition_start_timers,
	recog_request_stop
};

static recog_method_f recog_event_method_array[RECOGNIZER_EVENT_COUNT] = {
	recog_event_start_of_speech,
	recog_event_recognition_complete
};

/** Update state according to received incoming request from MRCP client */
static apt_bool_t recog_request_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	recog_method_f method;
	if(message->start_line.method_id >= RECOGNIZER_METHOD_COUNT) {
		return FALSE;
	}
	
	method = recog_request_method_array[message->start_line.method_id];
	return method(state_machine,message);
}

/** Update state according to received outgoing response from recognition engine */
static apt_bool_t recog_response_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
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
	return recog_message_dispatch(state_machine,message);
}

/** Update state according to received outgoing event from recognition engine */
static apt_bool_t recog_event_state_update(mrcp_recog_state_machine_t *state_machine, mrcp_message_t *message)
{
	recog_method_f method;
	if(message->start_line.method_id >= RECOGNIZER_EVENT_COUNT) {
		return FALSE;
	}
	
	method = recog_event_method_array[message->start_line.method_id];
	return method(state_machine,message);
}

/** Update state according to request received from MRCP client or response/event received from recognition engine */
static apt_bool_t recog_state_update(mrcp_state_machine_t *state_machine, mrcp_message_t *message)
{
	mrcp_recog_state_machine_t *recog_state_machine = (mrcp_recog_state_machine_t*)state_machine;
	apt_bool_t status = TRUE;
	switch(message->start_line.message_type) {
		case MRCP_MESSAGE_TYPE_REQUEST:
			status = recog_request_state_update(recog_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_RESPONSE:
			status = recog_response_state_update(recog_state_machine,message);
			break;
		case MRCP_MESSAGE_TYPE_EVENT:
			status = recog_event_state_update(recog_state_machine,message);
			break;
		default:
			status = FALSE;
			break;
	}
	return status;
}

/** Create MRCP recognizer server state machine */
mrcp_state_machine_t* mrcp_recog_server_state_machine_create(void *obj, mrcp_message_dispatcher_f dispatcher, apr_pool_t *pool)
{
	mrcp_recog_state_machine_t *state_machine = apr_palloc(pool,sizeof(mrcp_recog_state_machine_t));
	mrcp_state_machine_init(&state_machine->base,obj,dispatcher);
	state_machine->base.update = recog_state_update;
	state_machine->state = RECOGNIZER_STATE_IDLE;
	state_machine->active_request = NULL;
	state_machine->recog = NULL;
	state_machine->queue = apt_list_create(pool);
	mrcp_message_header_init(&state_machine->properties);
	return &state_machine->base;
}
