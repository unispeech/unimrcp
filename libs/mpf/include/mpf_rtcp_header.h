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

#ifndef __MPF_RTCP_HEADER_H__
#define __MPF_RTCP_HEADER_H__

/**
 * @file mpf_rtcp_header.h
 * @brief RTCP Header Definition
 */ 

#include "mpf.h"

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

/** RTCP header */
struct rtcp_header_t {
#if (APR_IS_BIGENDIAN == 1)
	/** protocol version */
	apr_uint32_t version: 2;
	/** padding flag */
	apr_uint32_t padding: 1;
	/** varies by packet type */
	apr_uint32_t count:   5;
	/** packet type */
	apr_uint32_t pt:      8;
#else
	/** varies by packet type */
	apr_uint32_t count:   5;
	/** padding flag */
	apr_uint32_t padding: 1;
	/** protocol version */
	apr_uint32_t version: 2;
	/** packet type */
	apr_uint32_t pt:      8;
#endif	
	
	/** packet length in words, w/o this word */
	apr_uint32_t length: 16;
};

APT_END_EXTERN_C

#endif /*__MPF_RTCP_HEADER_H__*/
