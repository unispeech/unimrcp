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

#ifndef __MPF_RTP_DEFS_H__
#define __MPF_RTP_DEFS_H__

/**
 * @file mpf_rtp_defs.h
 * @brief Internal RTP Definitions
 */ 

#include "mpf_rtp_stat.h"
#include "mpf_jitter_buffer.h"

APT_BEGIN_EXTERN_C

#define RTP_SEQ_MOD (1 << 16)
#define MAX_DROPOUT 3000
#define MAX_MISORDER 100

#define DISCARDED_TO_RECEIVED_RATIO_THRESHOLD 30 /* 30% */
#define DEVIATION_THRESHOLD 4000

typedef struct rtp_rx_history_t rtp_rx_history_t;
typedef struct rtp_rx_periodic_history_t rtp_rx_periodic_history_t;
typedef struct rtp_receiver_t rtp_receiver_t;
typedef struct rtp_transmitter_t rtp_transmitter_t;

/** RTP receive history */
struct rtp_rx_history_t {
	apr_uint32_t           seq_cycles;

	apr_uint16_t           seq_num_base;
	apr_uint16_t           seq_num_max;

	apr_uint32_t           ts_last;
	apr_time_t             time_last;

	apr_uint32_t           jitter_min;
	apr_uint32_t           jitter_max;

	apr_uint32_t           ssrc_new;
	apr_byte_t             ssrc_probation;
};

/** RTP receive periodic history */
struct rtp_rx_periodic_history_t {
	apr_uint32_t           received_prior;
	apr_uint32_t           discarded_prior;

	apr_uint32_t           jitter_min;
	apr_uint32_t           jitter_max;
};

/** Reset RTP receive history */
static APR_INLINE void mpf_rtp_rx_history_reset(rtp_rx_history_t *rx_history)
{
	memset(rx_history,0,sizeof(rtp_rx_history_t));
}

/** Reset RTP receive periodic history */
static APR_INLINE void mpf_rtp_rx_periodic_history_reset(rtp_rx_periodic_history_t *rx_periodic_history)
{
	memset(rx_periodic_history,0,sizeof(rtp_rx_periodic_history_t));
}

/** RTP receiver */
struct rtp_receiver_t {
	apr_byte_t                event_pt;

	mpf_jitter_buffer_t      *jb;

	rtp_rx_stat_t             stat;
	rtp_rx_history_t          history;
	rtp_rx_periodic_history_t periodic_history;
};

/** Initialize RTP receiver */
static APR_INLINE void rtp_receiver_init(rtp_receiver_t *receiver)
{
	receiver->event_pt = 0;

	receiver->jb = NULL;

	mpf_rtp_rx_stat_reset(&receiver->stat);
	mpf_rtp_rx_history_reset(&receiver->history);
	mpf_rtp_rx_periodic_history_reset(&receiver->periodic_history);
}


/** RTP transmitter */
struct rtp_transmitter_t {
	apr_uint32_t    ssrc;
	apr_byte_t      event_pt;
	apr_uint16_t    ptime;

	apr_uint16_t    packet_frames;
	apr_uint16_t    current_frames;
	apr_uint32_t    samples_per_frame;

	apr_byte_t      marker;
	apr_uint16_t    last_seq_num;
	apr_uint32_t    timestamp;

	char           *packet_data;
	apr_size_t      packet_size;

	rtp_tx_stat_t   stat;
};

/** Initialize RTP transmitter */
static APR_INLINE void rtp_transmitter_init(rtp_transmitter_t *transmitter)
{
	transmitter->ssrc = 0;
	transmitter->event_pt = 0;
	transmitter->ptime = 0;

	transmitter->packet_frames = 0;
	transmitter->current_frames = 0;
	transmitter->samples_per_frame = 0;

	transmitter->marker = 0;
	transmitter->last_seq_num = 0;
	transmitter->timestamp = 0;

	transmitter->packet_data = NULL;
	transmitter->packet_size = 0;

	mpf_rtp_tx_stat_reset(&transmitter->stat);
}

APT_END_EXTERN_C

#endif /*__MPF_RTP_DEFS_H__*/
