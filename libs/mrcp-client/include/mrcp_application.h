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

#ifndef __MRCP_APPLICATION_H__
#define __MRCP_APPLICATION_H__

/**
 * @file mrcp_application.h
 * @brief MRCP User Level Application Interface
 */ 

#include "mrcp_sig_types.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP application declaration */
typedef struct mrcp_application_t mrcp_application_t;

/** 
 * Create client session.
 * @param application the entire application
 * @param object the external object
 * @return the created session instance
 */
mrcp_session_t* mrcp_client_session_create(mrcp_application_t *application, void *object);

/** 
 * Destroy client session (session must be terminated prior to destroy).
 * @param application the entire application
 * @param session the session to destroy
 */
apt_bool_t mrcp_client_session_destroy(mrcp_application_t *application, mrcp_session_t *session);

/** 
 * Send session termination request.
 * @param application the entire application
 * @param session the session to terminate
 */
apt_bool_t mrcp_client_session_terminate(mrcp_application_t *application, mrcp_session_t *session);


APT_END_EXTERN_C

#endif /*__MRCP_APPLICATION_H__*/
