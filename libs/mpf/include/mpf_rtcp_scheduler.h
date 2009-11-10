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

#ifndef __MPF_RTCP_SCHEDULER_H__
#define __MPF_RTCP_SCHEDULER_H__

/**
 * @file mpf_rtcp_scheduler.h
 * @brief RTCP Session Scheduler
 */ 

#include "mpf_rtp_defs.h"
#include "mpf_rtcp_header.h"

APT_BEGIN_EXTERN_C

/** RTCP scheduler declaration */
typedef struct rtcp_scheduler_t rtcp_scheduler_t;

/** RTCP session declaration */
typedef struct rtcp_session_t rtcp_session_t;

/** Create RTCP scheduler */
MPF_DECLARE(rtcp_scheduler_t*) mpf_rtcp_scheduler_create(apr_pool_t *pool);

/** Process RTCP scheduler tasks */
MPF_DECLARE(void) mpf_rtcp_scheduler_process(rtcp_scheduler_t *scheduler);

/** Create RTCP session */
MPF_DECLARE(rtcp_session_t*) mpf_rtcp_session_create(
								rtcp_scheduler_t *scheduler,
								rtp_receiver_t *receiver, 
								rtp_transmitter_t *transmitter,
								apr_pool_t *pool);

/** Destroy RTCP session */
MPF_DECLARE(void) mpf_rtcp_session_destroy(
								rtcp_scheduler_t *scheduler,
								rtcp_session_t *rtcp_session);

APT_END_EXTERN_C

#endif /*__MPF_RTCP_SCHEDULER_H__*/
