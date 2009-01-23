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

#include "mrcp_stream.h"
#include "mrcp_message.h"
#include "mrcp_resource_factory.h"

/** MRCP parser */
struct mrcp_parser_t {
	mrcp_resource_factory_t *resource_factory;
	mrcp_stream_result_e     result;
	char                    *pos;
	mrcp_message_t          *message;
	apr_pool_t              *pool;
};

/** MRCP generator */
struct mrcp_generator_t {
	mrcp_resource_factory_t *resource_factory;
	mrcp_stream_result_e     result;
	char                    *pos;
	mrcp_message_t          *message;
	apr_pool_t              *pool;
};


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_parser_t *parser = apr_palloc(pool,sizeof(mrcp_parser_t));
	parser->resource_factory = resource_factory;
	parser->result = MRCP_STREAM_MESSAGE_INVALID;
	parser->pos = NULL;
	parser->message = NULL;
	parser->pool = pool;
	return parser;
}

static mrcp_stream_result_e mrcp_parser_break(mrcp_parser_t *parser, apt_text_stream_t *stream)
{
	/* failed to parse either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = parser->pos;
		parser->result = MRCP_STREAM_MESSAGE_TRUNCATED;
		parser->message = NULL;
	}
	else {
		/* error case */
		parser->result = MRCP_STREAM_MESSAGE_INVALID;
	}
	return parser->result;
}

/** Parse MRCP stream */
MRCP_DECLARE(mrcp_stream_result_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream)
{
	mrcp_message_t *message = parser->message;
	if(message && parser->result == MRCP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		parser->result = MRCP_STREAM_MESSAGE_COMPLETE;
//		if(mrcp_message_body_read(message,stream) == FALSE) {
//			parser->result = MRCP_STREAM_MESSAGE_TRUNCATED;
//		}
		return parser->result;
	}
	
	/* create new MRCP message */
	message = mrcp_message_create(parser->pool);
	parser->message = message;
	/* store current position to be able to rewind/restore stream if needed */
	parser->pos = stream->pos;
	/* parse start-line */
	if(mrcp_start_line_parse(&message->start_line,stream,message->pool) == FALSE) {
		return mrcp_parser_break(parser,stream);
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_parse(&message->channel_id,stream,message->pool);
	}

	if(mrcp_message_resourcify_by_name(parser->resource_factory,message) == FALSE) {
		return MRCP_STREAM_MESSAGE_INVALID;
	}

	/* parse header */
	if(mrcp_message_header_parse(&message->header,stream,message->pool) == FALSE) {
		return mrcp_parser_break(parser,stream);
	}

	/* parse body */
	parser->result = MRCP_STREAM_MESSAGE_COMPLETE;
	if(mrcp_body_parse(message,stream,message->pool) == FALSE) {
		parser->result = MRCP_STREAM_MESSAGE_TRUNCATED;
	}
	return parser->result;
}

/** Get parsed MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_parser_message_get(const mrcp_parser_t *parser)
{
	return parser->message;
}


/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_generator_t *generator = apr_palloc(pool,sizeof(mrcp_generator_t));
	generator->resource_factory = resource_factory;
	generator->result = MRCP_STREAM_MESSAGE_INVALID;
	generator->pos = NULL;
	generator->message = NULL;
	generator->pool = pool;
	return generator;
}

/** Set MRCP message to generate */
MRCP_DECLARE(apt_bool_t) mrcp_generator_message_set(mrcp_generator_t *generator, mrcp_message_t *message)
{
	if(!message) {
		return FALSE;
	}
	generator->message = message;
	return TRUE;
}

static mrcp_stream_result_e mrcp_generator_break(mrcp_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate either start-line or header */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached, rewind/restore stream */
		stream->pos = generator->pos;
		generator->result = MRCP_STREAM_MESSAGE_TRUNCATED;
	}
	else {
		/* error case */
		generator->result = MRCP_STREAM_MESSAGE_INVALID;
	}
	return generator->result;
}

/** Generate MRCP stream */
MRCP_DECLARE(mrcp_stream_result_e) mrcp_generator_run(mrcp_generator_t *generator, apt_text_stream_t *stream)
{
	mrcp_message_t *message = generator->message;
	if(!message) {
		return MRCP_STREAM_MESSAGE_INVALID;
	}

	if(message && generator->result == MRCP_STREAM_MESSAGE_TRUNCATED) {
		/* process continuation data */
		generator->result = MRCP_STREAM_MESSAGE_COMPLETE;
//		if(mrcp_message_body_write(message,stream) == FALSE) {
//			generator->result = MRCP_STREAM_MESSAGE_TRUNCATED;
//		}
		return generator->result;
	}

	/* initialize resource specific data */
	if(mrcp_message_resourcify_by_id(generator->resource_factory,message) == FALSE) {
		return MRCP_STREAM_MESSAGE_INVALID;
	}

	/* validate */
	if(mrcp_message_validate(message) == FALSE) {
		return MRCP_STREAM_MESSAGE_INVALID;
	}
	
	/* generate start-line */
	if(mrcp_start_line_generate(&message->start_line,stream) == FALSE) {
		return mrcp_generator_break(generator,stream);
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&message->channel_id,stream);
	}

	/* generate header */
	if(mrcp_message_header_generate(&message->header,stream) == FALSE) {
		return mrcp_generator_break(generator,stream);
	}

	/* generate body */
	mrcp_body_generate(message,stream);
	
	stream->text.length = stream->pos - stream->text.buf;
	mrcp_start_line_finalize(&message->start_line,stream);
	return generator->result;
}
