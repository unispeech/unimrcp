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

#ifndef __RTSP_MESSAGE_H__
#define __RTSP_MESSAGE_H__

/**
 * @file rtsp_message.h
 * @brief RTSP Message Definition
 */ 

#include "rtsp_start_line.h"
#include "rtsp_header.h"

APT_BEGIN_EXTERN_C

/** RTSP message declaration */
typedef struct rtsp_message_t rtsp_message_t;

/** RTSP message */
struct rtsp_message_t {
	/** RTSP mesage type (request/response) */
	rtsp_start_line_t start_line;     
	/** RTSP header */
	rtsp_header_t     header;
	/** RTSP message body */
	apt_str_t         body;

	/** Pool to allocate memory from */
	apr_pool_t       *pool;
};

/** Initialize RTSP message */
static APR_INLINE void rtsp_message_init(rtsp_message_t *message, rtsp_message_type_e message_type, apr_pool_t *pool)
{
	message->pool = pool;
	rtsp_start_line_init(&message->start_line,message_type);
	rtsp_header_init(&message->header);
	apt_string_reset(&message->body);
}

/** Create RTSP message */
static APR_INLINE rtsp_message_t* rtsp_message_create(rtsp_message_type_e message_type, apr_pool_t *pool)
{
	rtsp_message_t *message = apr_palloc(pool,sizeof(rtsp_message_t));
	rtsp_message_init(message,message_type,pool);
	return message;
}

/** 
 * Create RTSP request message.
 * @param engine the engine
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_request_create(apr_pool_t *pool);

/** 
 * Create RTSP response message.
 * @param engine the engine
 * @param request the request to create response to
 * @param status_code the status code of the response
 * @param reason the reason phrase of the response
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(rtsp_message_t *request, rtsp_status_code_e status_code, const char *reason, apr_pool_t *pool);

/** Destroy MRCP message */
RTSP_DECLARE(void) rtsp_message_destroy(rtsp_message_t *message);

/** Parse RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_parse(rtsp_message_t *message, apt_text_stream_t *text_stream);

/** Generate RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_generate(rtsp_message_t *message, apt_text_stream_t *text_stream);

APT_END_EXTERN_C

#endif /*__RTSP_MESSAGE_H__*/
