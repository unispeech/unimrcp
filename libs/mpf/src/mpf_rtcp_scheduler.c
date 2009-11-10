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

#include <apr_hash.h>
#include "mpf_rtcp_scheduler.h"

/** RTCP scheduler */
struct rtcp_scheduler_t {
	apr_hash_t *session_table;
};

/** RTCP session */
struct rtcp_session_t {
	rtp_receiver_t    *receiver;
	rtp_transmitter_t *transmitter;
};

/** Create RTCP scheduler */
MPF_DECLARE(rtcp_scheduler_t*) mpf_rtcp_scheduler_create(apr_pool_t *pool)
{
	rtcp_scheduler_t *scheduler = apr_palloc(pool,sizeof(rtcp_scheduler_t));
	scheduler->session_table = apr_hash_make(pool);
	return scheduler;
}

/** Process RTCP scheduler tasks */
MPF_DECLARE(void) mpf_rtcp_scheduler_process(rtcp_scheduler_t *scheduler)
{
}

/** Create RTCP session */
MPF_DECLARE(rtcp_session_t*) mpf_rtcp_session_create(
								rtcp_scheduler_t *scheduler,
								rtp_receiver_t *receiver, 
								rtp_transmitter_t *transmitter,
								apr_pool_t *pool)
{
	rtcp_session_t *rtcp_session = apr_palloc(pool,sizeof(rtcp_session_t));
	rtcp_session->receiver = receiver;
	rtcp_session->transmitter = transmitter;

	apr_hash_set(scheduler->session_table,rtcp_session,sizeof(rtcp_session),rtcp_session);
	return rtcp_session;
}

/** Destroy RTCP session */
MPF_DECLARE(void) mpf_rtcp_session_destroy(
								rtcp_scheduler_t *scheduler,
								rtcp_session_t *rtcp_session)
{
	apr_hash_set(scheduler->session_table,rtcp_session,sizeof(rtcp_session),NULL);
}
