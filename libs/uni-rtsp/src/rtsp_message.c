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

/** RTSP parser */
struct rtsp_parser_t {
	rtsp_stream_result_e result;
	char                *pos;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** RTSP generator */
struct rtsp_generator_t {
	rtsp_stream_result_e result;
	char                *pos;
	rtsp_message_t      *message;
	apr_pool_t          *pool;
};

/** Read RTSP message-body */
static apt_bool_t rtsp_message_body_read(rtsp_message_t *message, apt_text_stream_t *stream)
{
	apt_bool_t result = TRUE;
	if(message->body.buf) {
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = FALSE;
		}
		memcpy(message->body.buf+message->body.length,stream->pos,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
	}

	return result;
}

/** Parse RTSP message-body */
static apt_bool_t rtsp_message_body_parse(rtsp_message_t *message, apt_text_stream_t *stream, apr_pool_t *pool)
{
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		if(message->header.content_length) {
			apt_str_t *body = &message->body;
			body->buf = apr_palloc(pool,message->header.content_length+1);
			body->length = 0;
			return rtsp_message_body_read(message,stream);
		}
	}
	return TRUE;
}

/** Write RTSP message-body */
static apt_bool_t rtsp_message_body_write(rtsp_message_t *message, apt_text_stream_t *stream)
{
	apt_bool_t result = TRUE;
	if(message->body.length < message->header.content_length) {
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = message->header.content_length - message->body.length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* not complete */
			result = FALSE;
		}

		memcpy(stream->pos,message->body.buf+message->body.length,required_length);
		message->body.length += required_length;
		stream->pos += required_length;
	}

	return result;
}

/** Generate RTSP message-body */
static apt_bool_t rtsp_message_body_generate(rtsp_message_t *message, apt_text_stream_t *stream, apr_pool_t *pool)
{
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		if(message->header.content_length) {
			apt_str_t *body = &message->body;
			body->length = 0;
			return rtsp_message_body_write(message,stream);
		}
	}
	return TRUE;
}



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
RTSP_DECLARE(rtsp_message_t*) rtsp_response_create(const rtsp_message_t *request, rtsp_status_code_e status_code, rtsp_reason_phrase_e reason, apr_pool_t *pool)
{
	const apt_str_t *reason_str;
	rtsp_status_line_t *status_line;
	rtsp_message_t *response = rtsp_message_create(RTSP_MESSAGE_TYPE_RESPONSE,request->pool);
	status_line = &response->start_line.common.status_line;
	status_line->version = request->start_line.common.request_line.version;
	status_line->status_code = status_code;
	reason_str = rtsp_reason_phrase_get(reason);
	if(reason_str) {
		apt_string_copy(&status_line->reason,reason_str,request->pool);
	}

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


/** Create RTSP parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool)
{
	rtsp_parser_t *parser = apr_palloc(pool,sizeof(rtsp_parser_t));
	parser->result = RTSP_STREAM_MESSAGE_INVALID;
	parser->pos = NULL;
	parser->message = NULL;
	parser->pool = pool;
	return parser;
}

static rtsp_stream_result_e rtsp_parser_break(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	/* failed to parse either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = parser->pos;
		parser->result = RTSP_STREAM_MESSAGE_TRUNCATED;
		parser->message = NULL;
	}
	else {
		/* error case */
		parser->result = RTSP_STREAM_MESSAGE_INVALID;
	}
	return parser->result;
}

/** Parse RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream)
{
	rtsp_message_t *message = parser->message;
	if(message && parser->result == RTSP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		parser->result = RTSP_STREAM_MESSAGE_COMPLETE;
		if(rtsp_message_body_read(message,stream) == FALSE) {
			parser->result = RTSP_STREAM_MESSAGE_TRUNCATED;
		}
		return parser->result;
	}

	/* create new RTSP message */
	message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,parser->pool);
	parser->message = message;
	/* store current position to be able to rewind/restore stream if needed */
	parser->pos = stream->pos;
	/* parse start-line */
	if(rtsp_start_line_parse(&message->start_line,stream,message->pool) == FALSE) {
		return rtsp_parser_break(parser,stream);
	}
	/* parse header */
	if(rtsp_header_parse(&message->header,stream,message->pool) == FALSE) {
		return rtsp_parser_break(parser,stream);
	}
	/* parse body */
	parser->result = RTSP_STREAM_MESSAGE_COMPLETE;
	if(rtsp_message_body_parse(message,stream,message->pool) == FALSE) {
		parser->result = RTSP_STREAM_MESSAGE_TRUNCATED;
	}
	return parser->result;
}

/** Get parsed RTSP message */
RTSP_DECLARE(rtsp_message_t*) rtsp_parser_message_get(const rtsp_parser_t *parser)
{
	return parser->message;
}


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool)
{
	rtsp_generator_t *generator = apr_palloc(pool,sizeof(rtsp_generator_t));
	generator->result = RTSP_STREAM_MESSAGE_INVALID;
	generator->pos = NULL;
	generator->message = NULL;
	generator->pool = pool;
	return generator;
}

/** Set RTSP message to generate */
RTSP_DECLARE(apt_bool_t) rtsp_generator_message_set(rtsp_generator_t *generator, rtsp_message_t *message)
{
	if(!message) {
		return FALSE;
	}
	generator->message = message;
	return TRUE;
}

static rtsp_stream_result_e rtsp_generator_break(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = generator->pos;
		generator->result = RTSP_STREAM_MESSAGE_TRUNCATED;
	}
	else {
		/* error case */
		generator->result = RTSP_STREAM_MESSAGE_INVALID;
	}
	return generator->result;
}

/** Generate RTSP stream */
RTSP_DECLARE(rtsp_stream_result_e) rtsp_generator_run(rtsp_generator_t *generator, apt_text_stream_t *stream)
{
	rtsp_message_t *message = generator->message;
	if(!message) {
		return RTSP_STREAM_MESSAGE_INVALID;
	}

	if(message && generator->result == RTSP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		generator->result = RTSP_STREAM_MESSAGE_COMPLETE;
		if(rtsp_message_body_write(message,stream) == FALSE) {
			generator->result = RTSP_STREAM_MESSAGE_TRUNCATED;
		}
		return generator->result;
	}

	/* generate start-line */
	if(rtsp_start_line_generate(&message->start_line,stream) == FALSE) {
		return rtsp_generator_break(generator,stream);
	}

	/* generate header */
	if(rtsp_header_generate(&message->header,stream) == FALSE) {
		return rtsp_generator_break(generator,stream);
	}

	/* generate body */
	generator->result = RTSP_STREAM_MESSAGE_COMPLETE;
	if(rtsp_message_body_generate(message,stream,message->pool) == FALSE) {
		generator->result = RTSP_STREAM_MESSAGE_TRUNCATED;
	}
	return generator->result;
}
