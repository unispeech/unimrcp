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

#include "rtsp_message.h"

/** Parse RTSP message-body */
static apt_bool_t rtsp_body_parse(rtsp_message_t *message, apt_text_stream_t *text_stream, apr_pool_t *pool)
{
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		if(message->header.content_length) {
			apt_str_t *body = &message->body;
			body->length = message->header.content_length;
			if(body->length > (text_stream->text.length - (text_stream->pos - text_stream->text.buf))) {
				body->length = text_stream->text.length - (text_stream->pos - text_stream->text.buf);
			}
			body->buf = apr_pstrmemdup(pool,text_stream->pos,body->length);
			text_stream->pos += body->length;
		}
	}
	return TRUE;
}

/** Generate RTSP message-body */
static apt_bool_t rtsp_body_generate(rtsp_message_t *message, apt_text_stream_t *text_stream)
{
	apt_str_t *body = &message->body;
	if(body->length) {
		memcpy(text_stream->pos,body->buf,body->length);
		text_stream->pos += body->length;
	}
	return TRUE;
}

/** Create RTSP request message */
RTSP_DECLARE(rtsp_message_t*) rtsp_request_create(apr_pool_t *pool)
{
	rtsp_message_t *request = rtsp_message_create(RTSP_MESSAGE_TYPE_REQUEST,pool);
	request->start_line.common.request_line.version = RTSP_VERSION_1;
	return request;
}

/** Create RTSP response message */
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(const rtsp_message_t *request, rtsp_status_code_e status_code, const char *reason, apr_pool_t *pool)
{
	rtsp_status_line_t *status_line;
	rtsp_message_t *response = rtsp_message_create(RTSP_MESSAGE_TYPE_RESPONSE,request->pool);
	status_line = &response->start_line.common.status_line;
	status_line->version = request->start_line.common.request_line.version;
	status_line->status_code = status_code;
	apt_string_assign(&status_line->reason,reason,pool);

	if(rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_CSEQ) == TRUE) {
		response->header.cseq = request->header.cseq;
		rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CSEQ);
	}

	return response;
}

/** Destroy RTSP message */
RTSP_DECLARE(void) rtsp_message_destroy(rtsp_message_t *message)
{
	/* nothing to do message is allocated from pool */
}

/** Parse RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_parse(rtsp_message_t *message, apt_text_stream_t *text_stream)
{
	if(rtsp_start_line_parse(&message->start_line,text_stream,message->pool) == FALSE) {
		return FALSE;
	}

	if(rtsp_header_parse(&message->header,text_stream,message->pool) == FALSE) {
		return FALSE;
	}

	return rtsp_body_parse(message,text_stream,message->pool);
}

/** Generate RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_message_generate(rtsp_message_t *message, apt_text_stream_t *text_stream)
{
	if(rtsp_start_line_generate(&message->start_line,text_stream) == FALSE) {
		return FALSE;
	}

	if(rtsp_header_generate(&message->header,text_stream) == FALSE) {
		return FALSE;
	}

	rtsp_body_generate(message,text_stream);
	text_stream->text.length = text_stream->pos - text_stream->text.buf;
	return TRUE;
}
