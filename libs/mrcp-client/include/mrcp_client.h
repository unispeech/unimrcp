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

#ifndef __MRCP_CLIENT_H__
#define __MRCP_CLIENT_H__

/**
 * @file mrcp_client.h
 * @brief MRCP Client
 */ 

#include "mrcp_sig_types.h"
#include "mpf_types.h"
#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Opaque MRCP client declaration */
typedef struct mrcp_client_t mrcp_client_t;

/**
 * Create MRCP client instance.
 * @return the created client instance
 */
MRCP_DECLARE(mrcp_client_t*) mrcp_client_create();

/**
 * Start message processing loop.
 * @param client the MRCP client to start
 * @return the created client instance
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_start(mrcp_client_t *client);

/**
 * Shutdown message processing loop.
 * @param client the MRCP client to shutdown
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_shutdown(mrcp_client_t *client);

/**
 * Destroy MRCP client.
 * @param client the MRCP client to destroy
 */
MRCP_DECLARE(apt_bool_t) mrcp_client_destroy(mrcp_client_t *client);


APT_END_EXTERN_C

#endif /*__MRCP_CLIENT_H__*/
