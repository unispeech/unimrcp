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

#ifndef __MPF_MESSAGE_H__
#define __MPF_MESSAGE_H__

/**
 * @file mpf_message.h
 * @brief Media Processing Framework Message Definitions
 */ 

#include "mpf.h"

APT_BEGIN_EXTERN_C

/** Enumeration of MPF message types */
typedef enum {
	MPF_MESSAGE_TYPE_REQUEST,  /**< request message */
	MPF_MESSAGE_TYPE_RESPONSE, /**< response message */
	MPF_MESSAGE_TYPE_EVENT     /**< event message */
} mpf_message_type_e;

/** Enumeration of MPF status codes */
typedef enum {
	MPF_STATUS_CODE_SUCCESS,  /**< indicates success */
	MPF_STATUS_CODE_FAILURE   /**< indicates failure */
} mpf_status_code_e;


/** Enumeration of action types */
typedef enum {
	MPF_ACTION_TYPE_CONTEXT,    /**< context specific action */
	MPF_ACTION_TYPE_TERMINATION /**< termination specific action */
} mpf_action_type_e;

/** Enumeration of context actions */
typedef enum {
	MPF_CONTEXT_ACTION_ADD,      /**< add termination to context */
	MPF_CONTEXT_ACTION_SUBTRACT, /**< subtract termination from context */
	MPF_CONTEXT_ACTION_MOVE      /**< move termination to another context */
} mpf_context_action_e;

/** Opaque MPF context declaration */
typedef struct mpf_context_t mpf_context_t;

/** Opaque MPF termination declaration */
typedef struct mpf_termination_t mpf_termination_t;

/** MPF message declaration */
typedef struct mpf_message_t mpf_message_t;

/** MPF message definition */
struct mpf_message_t {
	mpf_message_type_e message_type;
	mpf_action_type_e  action_type;
	int                action_id;
	mpf_status_code_e  status_code;

	mpf_context_t     *context;
	mpf_termination_t *termination;
};


APT_END_EXTERN_C

#endif /*__MPF_MESSAGE_H__*/
