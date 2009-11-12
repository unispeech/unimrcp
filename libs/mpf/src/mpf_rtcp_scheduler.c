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

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h> 
#include <apr_hash.h>
#include "mpf_rtcp_scheduler.h"

/** RTCP scheduler */
struct mpf_rtcp_scheduler_t {
	/** Ring head */
	APR_RING_HEAD(rtcp_session_head_t, mpf_rtcp_session_t) head;

	apr_size_t  elapsed_time;
};

/** RTCP session */
struct mpf_rtcp_session_t {
	/** Ring entry */
	APR_RING_ENTRY(mpf_rtcp_session_t) link;

	rtp_receiver_t    *receiver;
	rtp_transmitter_t *transmitter;

	/* time next report is scheduled at */
	apr_size_t         scheduled_time;
};

static apt_bool_t mpf_rtcp_session_schedule(mpf_rtcp_scheduler_t *scheduler, mpf_rtcp_session_t *rtcp_session);
static apt_bool_t mpf_rtcp_report_receive(mpf_rtcp_session_t *session);
static apt_bool_t mpf_rtcp_report_transmit(mpf_rtcp_session_t *session);

/** Create RTCP scheduler */
MPF_DECLARE(mpf_rtcp_scheduler_t*) mpf_rtcp_scheduler_create(apr_pool_t *pool)
{
	mpf_rtcp_scheduler_t *scheduler = apr_palloc(pool,sizeof(mpf_rtcp_scheduler_t));
	APR_RING_INIT(&scheduler->head, mpf_rtcp_session_t, link);
	scheduler->elapsed_time = 0;
	return scheduler;
}

/** Process RTCP scheduler tasks */
MPF_DECLARE(void) mpf_rtcp_scheduler_process(mpf_rtcp_scheduler_t *scheduler)
{
	mpf_rtcp_session_t *session;
	mpf_rtcp_session_t *elapsed_session;

	if(APR_RING_EMPTY(&scheduler->head, mpf_rtcp_session_t, link)) {
		/* just return, nothing to do */
		return;
	}

	/* increment elapsed time */
	scheduler->elapsed_time += CODEC_FRAME_TIME_BASE;

	/* process RTCP sessions */
	session = APR_RING_FIRST(&scheduler->head);
	do {
		if(session->scheduled_time > scheduler->elapsed_time) {
			/* scheduled time is not elapsed yet */
			break;
		}
		
		/* scheduled time is elapsed */
		elapsed_session = session;
		session = APR_RING_NEXT(session, link);

		/* remove the elapsed session from the list, process it and reschedule */
		APR_RING_REMOVE(elapsed_session, link);
		mpf_rtcp_report_transmit(elapsed_session);
		mpf_rtcp_session_schedule(scheduler,elapsed_session);
	}
	while(session != APR_RING_SENTINEL(&scheduler->head, mpf_rtcp_session_t, link));
}

/** Create RTCP session */
MPF_DECLARE(mpf_rtcp_session_t*) mpf_rtcp_session_create(
								mpf_rtcp_scheduler_t *scheduler,
								rtp_receiver_t *receiver, 
								rtp_transmitter_t *transmitter,
								apr_pool_t *pool)
{
	mpf_rtcp_session_t *rtcp_session = apr_palloc(pool,sizeof(mpf_rtcp_session_t));
	rtcp_session->receiver = receiver;
	rtcp_session->transmitter = transmitter;

	mpf_rtcp_session_schedule(scheduler,rtcp_session);
	return rtcp_session;
}

/** Destroy RTCP session */
MPF_DECLARE(void) mpf_rtcp_session_destroy(
								mpf_rtcp_scheduler_t *scheduler,
								mpf_rtcp_session_t *rtcp_session)
{
	APR_RING_REMOVE(rtcp_session,link);
}

static apt_bool_t mpf_rtcp_session_schedule(mpf_rtcp_scheduler_t *scheduler, mpf_rtcp_session_t *rtcp_session)
{
	mpf_rtcp_session_t *it;
	rtcp_session->scheduled_time = scheduler->elapsed_time + 5000;

	for(it = APR_RING_LAST(&scheduler->head);
			it != APR_RING_SENTINEL(&scheduler->head, mpf_rtcp_session_t, link);
				it = APR_RING_PREV(it, link)) {
		
		if(it->scheduled_time < rtcp_session->scheduled_time) {
			APR_RING_INSERT_AFTER(it,rtcp_session,link);
			break;
		}
	}
	return TRUE;
}

static apt_bool_t mpf_rtcp_report_receive(mpf_rtcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t mpf_rtcp_report_transmit(mpf_rtcp_session_t *session)
{
	return TRUE;
}
