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

/** Result of RTSP stream processing (parse/generate) */
typedef enum {
	RTSP_STREAM_MESSAGE_COMPLETE,
	RTSP_STREAM_MESSAGE_TRUNCATED,
	RTSP_STREAM_MESSAGE_INVALID
} rtsp_stream_result_e;

/** Opaque RTSP parser declaration */
typedef struct rtsp_parser_t rtsp_parser_t;
/** Opaque RTSP generator declaration */
typedef struct rtsp_generator_t rtsp_generator_t;
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
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_request_create(apr_pool_t *pool);

/** 
 * Create RTSP response message.
 * @param request the request to create response to
 * @param status_code the status code of the response
 * @param reason the reason phrase id of the response
 * @param pool the pool to allocate memory from
 */
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(const rtsp_message_t *request, rtsp_status_code_e status_code, rtsp_reason_phrase_e reason, apr_pool_t *pool);

/** 
 * Destroy RTSP message 
 * @param message the message to destroy
 */
RTSP_DECLARE(void) rtsp_message_destroy(rtsp_message_t *message);

/** Parse RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_parse(rtsp_message_t *message, apt_text_stream_t *stream);

/** Generate RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_generate(rtsp_message_t *message, apt_text_stream_t *stream);


/** Create RTSP stream parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool);

/** Parse RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream);

/** Get parsed RTSP message */
RTSP_DECLARE(rtsp_message_t*) rtsp_parser_message_get(const rtsp_parser_t *parser);


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool);

/** Set RTSP message to generate */
RTSP_DECLARE(apt_bool_t) rtsp_generator_message_set(rtsp_generator_t *generator, rtsp_message_t *message);

/** Generate RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_generator_run(rtsp_generator_t *generator, apt_text_stream_t *stream);


APT_END_EXTERN_C

#endif /*__RTSP_MESSAGE_H__*/
