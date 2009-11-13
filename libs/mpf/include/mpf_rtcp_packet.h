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

#ifndef __MPF_RTCP_PACKET_H__
#define __MPF_RTCP_PACKET_H__

/**
 * @file mpf_rtcp_packet.h
 * @brief RTCP Packet Definition
 */ 

#include "mpf_rtp_stat.h"

APT_BEGIN_EXTERN_C

typedef enum {
	RTCP_SR   = 200,
	RTCP_RR   = 201,
	RTCP_SDES = 202,
	RTCP_BYE  = 203,
	RTCP_APP  = 204
} rtcp_type_e;

typedef enum {
	RTCP_SDES_END   = 0,
	RTCP_SDES_CNAME = 1,
	RTCP_SDES_NAME  = 2,
	RTCP_SDES_EMAIL = 3,
	RTCP_SDES_PHONE = 4,
	RTCP_SDES_LOC   = 5,
	RTCP_SDES_TOOL  = 6,
	RTCP_SDES_NOTE  = 7,
	RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_e;

/** RTCP header declaration */
typedef struct rtcp_header_t rtcp_header_t;
/** RTCP packet declaration */
typedef struct rtcp_packet_t rtcp_packet_t;
/** SDES item declaration*/
typedef struct rtcp_sdes_item_t rtcp_sdes_item_t;


/** RTCP header */
struct rtcp_header_t {
#if (APR_IS_BIGENDIAN == 1)
	/** protocol version */
	unsigned int version: 2;
	/** padding flag */
	unsigned int padding: 1;
	/** varies by packet type */
	unsigned int count:   5;
	/** packet type */
	unsigned int pt:      8;
#else
	/** varies by packet type */
	unsigned int count:   5;
	/** padding flag */
	unsigned int padding: 1;
	/** protocol version */
	unsigned int version: 2;
	/** packet type */
	unsigned int pt:      8;
#endif	
	
	/** packet length in words, w/o this word */
	unsigned int length:  16;
};

/** SDES item */
struct rtcp_sdes_item_t {
	/** type of item (rtcp_sdes_type_t) */
	apr_byte_t type;
	/* length of item (in octets) */
	apr_byte_t length;
	/* text, not null-terminated */
	char       data[1];
};

/** RTCP packet */
struct rtcp_packet_t {
	/** common header */
	rtcp_header_t header;
	union {
		/** sender report (SR) */
		struct {
			/** sr stat */
			rtcp_sr_stat_t sr_stat;
			/** variable-length list rr stats */
			rtcp_rr_stat_t rr_stat[1];
		} sr;

		/** reception report (RR) */
		struct {
			/** receiver generating this report */
			apr_uint32_t   ssrc;
			/** variable-length list rr stats */
			rtcp_rr_stat_t rr_stat[1];
		} rr;

		/** source description (SDES) */
		struct {
			/** first SSRC/CSRC */
			apr_uint32_t     ssrc;
			/** list of SDES items */
			rtcp_sdes_item_t item[1];
		} sdes;

		/** BYE */
		struct {
			/** list of sources */
			apr_uint32_t ssrc[1];
			/* can't express trailing text for reason */
		} bye;
	} r;
};

APT_END_EXTERN_C

#endif /*__MPF_RTCP_PACKET_H__*/
