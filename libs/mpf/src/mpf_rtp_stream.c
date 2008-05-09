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

static APR_INLINE void mpf_rtp_rx_history_init(rtp_rx_history_t *rx_history)
{
	memset(rx_history,0,sizeof(rtp_rx_history_t));
}

static APR_INLINE void mpf_rtp_rx_periodic_history_init(rtp_rx_periodic_history_t *rx_periodic_history)
{
	memset(rx_periodic_history,0,sizeof(rtp_rx_periodic_history_t));
}


struct rtp_receiver_t {
	apr_byte_t                event_pt;

	mpf_jitter_buffer_t      *jb;

	rtp_rx_stat_t             stat;
	rtp_rx_history_t          history;
	rtp_rx_periodic_history_t periodic_history;
};

static APR_INLINE void rtp_receiver_init(rtp_receiver_t *receiver)
{
	receiver->event_pt = 0;

	receiver->jb = NULL;

	mpf_rtp_rx_stat_init(&receiver->stat);
	mpf_rtp_rx_history_init(&receiver->history);
	mpf_rtp_rx_periodic_history_init(&receiver->periodic_history);
}


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

	mpf_rtp_tx_stat_init(&transmitter->stat);
}



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


static apt_bool_t mpf_rtp_stream_destroy(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = (mpf_rtp_stream_t*)stream;
	if(rtp_stream->socket) {
		apr_socket_close(rtp_stream->socket);
		rtp_stream->socket = NULL;
	}
	
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_open(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = (mpf_rtp_stream_t*)stream;
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	if(!rtp_stream->socket || !rtp_stream->local_media) {
		return FALSE;
	}

	receiver->jb = mpf_jitter_buffer_create(
						NULL,
						rtp_stream->local_media->codec_list.codecs[0].sampling_rate,
						rtp_stream->pool);
	return TRUE;
}

static apt_bool_t mpf_rtp_rx_stream_close(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = (mpf_rtp_stream_t*)stream;
	rtp_receiver_t *receiver = &rtp_stream->receiver;
	receiver->stat.lost_packets = 0;
	if(receiver->stat.received_packets) {
		apr_uint32_t expected_packets = receiver->history.seq_cycles + 
			receiver->history.seq_num_max - receiver->history.seq_num_base + 1;
		if(expected_packets > receiver->stat.received_packets) {
			receiver->stat.lost_packets = expected_packets - receiver->stat.received_packets;
		}
	}

	mpf_jitter_buffer_destroy(receiver->jb);
	return TRUE;
}

static apt_bool_t mpf_rtp_stream_receive(mpf_audio_stream_t *stream, mpf_frame_t *frame)
{
	return TRUE;
}


static apt_bool_t mpf_rtp_tx_stream_open(mpf_audio_stream_t *stream)
{
	mpf_rtp_stream_t *rtp_stream = (mpf_rtp_stream_t*)stream;
	rtp_transmitter_t *transmitter = &rtp_stream->transmitter;
	if(!rtp_stream->socket || !rtp_stream->remote_media) {
		return FALSE;
	}

	if(!transmitter->ptime) {
		transmitter->ptime = 20;
	}
	transmitter->packet_frames = transmitter->ptime / CODEC_FRAME_TIME_BASE;
	transmitter->current_frames = 0;
//	transmitter->samples_per_frame = CODEC_FRAME_TIME_BASE * transmitter->codec.descriptor->sampling_rate / 1000;
	
	transmitter->marker = 1;
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

MPF_DECLARE(mpf_audio_stream_t*) mpf_rtp_stream_create(mpf_termination_t *termination, apr_pool_t *pool)
{
	mpf_rtp_stream_t *rtp_stream = apr_palloc(pool,sizeof(mpf_rtp_stream_t));
	rtp_stream->pool = pool;
	rtp_stream->local_media = NULL;
	rtp_stream->remote_media = NULL;
	rtp_stream->socket = NULL;
	rtp_stream->local_sockaddr = NULL;
	rtp_stream->remote_sockaddr = NULL;
	mpf_audio_stream_init(&rtp_stream->base,&vtable);
	rtp_stream->base.termination = termination;
	rtp_receiver_init(&rtp_stream->receiver);
	rtp_transmitter_init(&rtp_stream->transmitter);
	rtp_stream->transmitter.ssrc = (apr_uint32_t)apr_time_now();

	return &rtp_stream->base;
}


static apt_bool_t mpf_rtp_socket_create(mpf_rtp_stream_t *stream, mpf_rtp_media_descriptor_t *local_media)
{
	if(stream->socket) {
		apr_socket_close(stream->socket);
		stream->socket = NULL;
	}
	
	apr_sockaddr_info_get(&stream->local_sockaddr,local_media->ip,APR_INET,local_media->port,0,stream->pool);
	if(!stream->local_sockaddr) {
		return FALSE;
	}
	if(apr_socket_create(&stream->socket,stream->local_sockaddr->family,SOCK_DGRAM,0,stream->pool) != APR_SUCCESS) {
		return FALSE;
	}
	
	apr_socket_opt_set(stream->socket,APR_SO_NONBLOCK,1);
	apr_socket_timeout_set(stream->socket,0);
	apr_socket_opt_set(stream->socket,APR_SO_REUSEADDR,1);

	if(apr_socket_bind(stream->socket,stream->local_sockaddr) != APR_SUCCESS) {
		return FALSE;
	}
	return TRUE;
}

MPF_DECLARE(apt_bool_t) mpf_rtp_stream_modify(mpf_audio_stream_t *stream, mpf_rtp_stream_descriptor_t *descriptor)
{
	mpf_rtp_stream_t *rtp_stream = (mpf_rtp_stream_t*)stream;
	apt_bool_t status = TRUE;
	if(descriptor->mask & RTP_MEDIA_DESCRIPTOR_LOCAL) {
		/* update local media */
		mpf_rtp_media_descriptor_t *local_media = &descriptor->local;
		if(!rtp_stream->local_media || 
			apt_str_compare(rtp_stream->local_media->ip,local_media->ip) == FALSE ||
			rtp_stream->local_media->port != local_media->port) {

			if(mpf_rtp_socket_create(rtp_stream,local_media) == FALSE) {
				status = FALSE;
			}
		}
		rtp_stream->local_media = local_media;
	}
	if(descriptor->mask & RTP_MEDIA_DESCRIPTOR_REMOTE) {
		/* update remote media */
		mpf_rtp_media_descriptor_t *remote_media = &descriptor->remote;
		if(!rtp_stream->remote_media || 
			apt_str_compare(rtp_stream->remote_media->ip,remote_media->ip) == FALSE ||
			rtp_stream->remote_media->port != remote_media->port) {

			apr_sockaddr_info_get(&rtp_stream->remote_sockaddr,remote_media->ip,APR_INET,remote_media->port,0,rtp_stream->pool);
			if(!rtp_stream->remote_sockaddr) {
				status = FALSE;
			}
		}
		rtp_stream->remote_media = remote_media;
	}
	stream->mode = descriptor->mode;
	return TRUE;
}
