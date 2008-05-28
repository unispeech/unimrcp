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

#ifndef __MRCP_SESSION_H__
#define __MRCP_SESSION_H__

/**
 * @file mrcp_session.h
 * @brief Abstract MRCP Session
 */ 

#include "mrcp_sig_types.h"
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** MRCP session methods vtable declaration */
typedef struct mrcp_session_method_vtable_t mrcp_session_method_vtable_t;
/** MRCP session events vtable declaration */
typedef struct mrcp_session_event_vtable_t mrcp_session_event_vtable_t;

/** MRCP session */
struct mrcp_session_t {
	/** Memory pool to allocate memory from */
	apr_pool_t *pool;
	/** External object associated with agent */
	void       *obj;
	/** Session identifier */
	apt_str_t   session_id;

	/** Virtual methods */
	const mrcp_session_method_vtable_t *method_vtable;
	/** Virtual events */
	const mrcp_session_event_vtable_t  *event_vtable;
};

/** MRCP session methods vtable */
struct mrcp_session_method_vtable_t {
	/** Offer local description to remote party */
	apt_bool_t (*offer)(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
	/** Answer to offer, by setting up local description according to the remote one */
	apt_bool_t (*answer)(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
	/** Terminate session */
	apt_bool_t (*terminate)(mrcp_session_t *session);
};

/** MRCP session events vtable */
struct mrcp_session_event_vtable_t {
	/** Receive offer from remote party */
	apt_bool_t (*on_offer)(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
	/** Receive answer from remote party */
	apt_bool_t (*on_answer)(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
	/** On terminate session */
	apt_bool_t (*on_terminate)(mrcp_session_t *session);
};

/** Create new memory pool and allocate session object from the pool. */
MRCP_DECLARE(mrcp_session_t*) mrcp_session_create();

/** Destroy session and assosiated memory pool. */
MRCP_DECLARE(void) mrcp_session_destroy(mrcp_session_t *session);


/** Offer */
static APR_INLINE apt_bool_t mrcp_session_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	if(session->method_vtable->offer) {
		return session->method_vtable->offer(session,descriptor);
	}
	return FALSE;
}

/** Answer */
static APR_INLINE apt_bool_t mrcp_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	if(session->method_vtable->answer) {
		return session->method_vtable->answer(session,descriptor);
	}
	return FALSE;
}

/** Terminate */
static APR_INLINE apt_bool_t mrcp_session_terminate(mrcp_session_t *session)
{
	if(session->method_vtable->terminate) {
		return session->method_vtable->terminate(session);
	}
	return FALSE;
}

/** On offer */
static APR_INLINE apt_bool_t mrcp_session_on_offer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	if(session->event_vtable->on_offer) {
		return session->event_vtable->on_offer(session,descriptor);
	}
	return FALSE;
}

/** On answer */
static APR_INLINE apt_bool_t mrcp_session_on_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	if(session->event_vtable->on_answer) {
		return session->event_vtable->on_answer(session,descriptor);
	}
	return FALSE;
}

/** On terminate */
static APR_INLINE apt_bool_t mrcp_session_on_terminate(mrcp_session_t *session)
{
	if(session->event_vtable->on_terminate) {
		return session->event_vtable->on_terminate(session);
	}
	return FALSE;
}

APT_END_EXTERN_C

#endif /*__MRCP_SESSION_H__*/
