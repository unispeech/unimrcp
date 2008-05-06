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

#include <apr_network_io.h>
#include "mpf_rtp_stream.h"
#include "mpf_rtp_stat.h"
#include "mpf_jitter_buffer.h"
#include "apt_log.h"

#define RTP_SEQ_MOD (1 << 16)
#define MAX_DROPOUT 3000
#define MAX_MISORDER 100

#define DISCARDED_TO_RECEIVED_RATIO_THRESHOLD 30 /* 30% */
#define DEVIATION_THRESHOLD 4000

typedef struct rtp_rx_history_t rtp_rx_history_t;
typedef struct rtp_rx_periodic_history_t rtp_rx_periodic_history_t;
typedef struct rtp_receiver_t rtp_receiver_t;
typedef struct rtp_transmitter_t rtp_transmitter_t;

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

struct rtp_rx_periodic_history_t {
	apr_uint32_t           received_prior;
	apr_uint32_t           discarded_prior;

	apr_uint32_t           jitter_min;
	apr_uint32_t           jitter_max;
};

struct rtp_receiver_t {
	apr_byte_t                event_pt;

	mpf_jitter_buffer_t      *jb;
	mpf_jb_config_t           jb_config;

	rtp_rx_stat_t             stat;
	rtp_rx_history_t          history;
	rtp_rx_periodic_history_t periodic_history;
};

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


struct mpf_rtp_stream_t {
	mpf_audio_stream_t          base;

	mpf_rtp_media_descriptor_t *local_media;
	mpf_rtp_media_descriptor_t *remote_media;

	rtp_transmitter_t           transmitter;
	rtp_receiver_t              receiver;

	apr_socket_t               *socket;
	apr_sockaddr_t             *local_sockaddr;
	apr_sockaddr_t             *remote_sockaddr;
	
	apr_pool_t                 *pool;
};


static apt_bool_t rtp_transmitter_open(mpf_rtp_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t rtp_transmitter_close(mpf_rtp_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t rtp_receiver_open(mpf_rtp_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t rtp_receiver_close(mpf_rtp_stream_t *stream)
{
	return TRUE;
}


static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_stream_receive(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	return TRUE;
}


static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_tx_stream_close(mpf_audio_stream_t *stream)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_stream_transmit(mpf_audio_stream_t *stream, const mpf_frame_t *frame)
{
	return TRUE;
}

static const mpf_audio_stream_vtable_t vtable = {
	mpf_rtp_stream_destroy,
	mpf_rtp_rx_stream_open,
	mpf_rtp_rx_stream_close,
	mpf_rtp_stream_receive,
	mpf_rtp_tx_stream_open,
	mpf_rtp_tx_stream_close,
	mpf_rtp_stream_transmit
};

MPF_DECLARE(mpf_audio_stream_t*) mpf_rtp_stream_create(apr_pool_t *pool)
{
	mpf_rtp_stream_t *rtp_stream = apr_palloc(pool,sizeof(mpf_rtp_stream_t));
	rtp_stream->pool = pool;
	rtp_stream->local_media = NULL;
	rtp_stream->remote_media = NULL;
	rtp_stream->socket = NULL;
	rtp_stream->local_sockaddr = NULL;
	rtp_stream->remote_sockaddr = NULL;
	mpf_audio_stream_init(&rtp_stream->base,&vtable);

	return &rtp_stream->base;
}

MPF_DECLARE(apt_bool_t) mpf_rtp_stream_modify(mpf_rtp_stream_t *stream, mpf_rtp_stream_descriptor_t *descriptor)
{
	if(stream->base.mode & STREAM_MODE_SEND) {
		if(descriptor->mode & STREAM_MODE_SEND) {
			/* transmitter is already opened, update params if needed */
		}
		else {
			/* close transmitter */
			rtp_transmitter_close(stream);
		}
	}
	else {
		if(descriptor->mode & STREAM_MODE_SEND) {
			/* open transmitter */
			rtp_transmitter_open(stream);
		}
		else {
			/* transmitter is already closed, update params if needed */
		}
	}

	if(stream->base.mode & STREAM_MODE_RECEIVE) {
		if(descriptor->mode & STREAM_MODE_RECEIVE) {
			/* receiver is already opened, update params if needed */
		}
		else {
			/* close receiver */
			rtp_receiver_close(stream);
		}
	}
	else {
		if(descriptor->mode & STREAM_MODE_RECEIVE) {
			/* open receiver */
			rtp_receiver_open(stream);
		}
		else {
			/* receiver is already closed, update params if needed */
		}
	}
	return TRUE;
}
