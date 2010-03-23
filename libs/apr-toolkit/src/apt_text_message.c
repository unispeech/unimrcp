/*
 * Copyright 2008-2010 Arsen Chaloyan
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
 * 
 * $Id$
 */

#include "apt_text_message.h"

/** Stage of text message processing (parsing/generation) */
typedef enum {
	APT_MESSAGE_STAGE_START_LINE,
	APT_MESSAGE_STAGE_HEADER,
	APT_MESSAGE_STAGE_BODY
} apt_message_stage_e;


/** Text message parser */
struct apt_message_parser_t {
	const apt_message_parser_vtable_t *vtable;
	void               *obj;
	apr_pool_t         *pool;

	void               *message;
	apr_size_t          content_length;
	apt_str_t           body;
	apt_message_stage_e stage;
	apt_bool_t          skip_lf;
};

/** Text message generator */
struct apt_message_generator_t {
	const apt_message_generator_vtable_t *vtable;
	void                 *obj;
	apr_pool_t           *pool;

	void                 *message;
	apt_header_section_t *header;
	apt_str_t            *body;
	apr_size_t            content_length;
	apt_message_stage_e   stage;
};

/** Parse individual header field (name-value pair) */
APT_DECLARE(apt_header_field_t*) apt_header_field_parse(apt_text_stream_t *stream, apr_pool_t *pool)
{
	apr_size_t folding_length = 0;
	apr_array_header_t *folded_lines = NULL;
	apt_header_field_t *header_field;
	apt_str_t *line;
	apt_pair_t pair;
	/* read name-value pair */
	if(apt_text_header_read(stream,&pair) == FALSE) {
		return NULL;
	}

	/* check folding lines (value spanning multiple lines) */
	while(stream->pos < stream->end) {
		if(apt_text_is_wsp(*stream->pos) == FALSE) {
			break;
		}

		stream->pos++;

		/* skip further white spaces (if any) */
		apt_text_white_spaces_skip(stream);

		if(!folded_lines) {
			folded_lines = apr_array_make(pool,1,sizeof(apt_str_t));
		}
		line = apr_array_push(folded_lines);
		apt_text_line_read(stream,line);
		folding_length += line->length;
	};

	header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	/* copy parsed name of the header field */
	header_field->name.length = pair.name.length;
	header_field->name.buf = apr_palloc(pool, pair.name.length + 1);
	if(pair.name.length) {
		memcpy(header_field->name.buf, pair.name.buf, pair.name.length);
	}
	header_field->name.buf[header_field->name.length] = '\0';

	/* copy parsed value of the header field */
	header_field->value.length = pair.value.length + folding_length;
	header_field->value.buf = apr_palloc(pool, header_field->value.length + 1);
	if(pair.value.length) {
		memcpy(header_field->value.buf, pair.value.buf, pair.value.length);
	}
	
	if(folding_length) {
		int i;
		char *pos = header_field->value.buf + pair.value.length;
		/* copy parsed folding lines */
		for(i=0; i<folded_lines->nelts; i++) {
			line = &APR_ARRAY_IDX(folded_lines,i,apt_str_t);

			memcpy(pos,line->buf,line->length);
			pos += line->length;
		}
	}
	header_field->value.buf[header_field->value.length] = '\0';
	
	return header_field;
}

/** Generate individual header field (name-value pair) */
APT_DECLARE(apt_bool_t) apt_header_field_generate(const apt_header_field_t *header_field, apt_text_stream_t *stream)
{
	apt_text_header_name_generate(&header_field->name,stream);
	if(apt_string_is_empty(&header_field->value) == FALSE) {
		apt_string_value_generate(&header_field->value,stream);
	}
	apt_text_eol_insert(stream);
	return TRUE;
}

static apt_bool_t apt_header_section_parse(apt_message_parser_t *parser, apt_text_stream_t *stream)
{
	apt_header_field_t *header_field;
	apt_bool_t result = FALSE;

	do {
		header_field = apt_header_field_parse(stream,parser->pool);
		if(header_field) {
			if(apt_string_is_empty(&header_field->name) == FALSE) {
				/* normal header */
				parser->vtable->on_header_field(parser,parser->message,header_field);
			}
			else {
				/* empty header => exit */
				parser->content_length = 0;
				parser->vtable->on_header_separator(parser,parser->message,&parser->content_length);
				result = TRUE;
				break;
			}
		}
		else {
			/* malformed header => skip to the next one */
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	return result;
}

static apt_bool_t apt_header_section_generate(apt_header_section_t *header, apt_text_stream_t *stream)
{
	apt_header_field_t *header_field;
	for(header_field = APR_RING_FIRST(&header->ring);
			header_field != APR_RING_SENTINEL(&header->ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {
		apt_header_field_generate(header_field,stream);
	}

	apt_text_eol_insert(stream);
	return TRUE;
}

static apt_bool_t apt_message_body_read(apt_message_parser_t *parser, apt_text_stream_t *stream)
{
	apt_bool_t status = TRUE;
	apt_str_t *body = &parser->body;
	if(body->buf) {
		/* stream length available to read */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to read */
		apr_size_t required_length = parser->content_length - body->length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* incomplete */
			status = FALSE;
		}
		memcpy(body->buf + body->length, stream->pos, required_length);
		body->length += required_length;
		stream->pos += required_length;
	}

	return status;
}

static apt_bool_t apt_message_body_write(apt_message_generator_t *generator, apt_text_stream_t *stream)
{
	apt_bool_t status = TRUE;
	if(generator->body && generator->body->length < generator->content_length) {
		/* stream length available to write */
		apr_size_t stream_length = stream->text.length - (stream->pos - stream->text.buf);
		/* required/remaining length to write */
		apr_size_t required_length = generator->content_length - generator->body->length;
		if(required_length > stream_length) {
			required_length = stream_length;
			/* incomplete */
			status = FALSE;
		}

		memcpy(stream->pos, generator->body->buf + generator->body->length, required_length);
		generator->body->length += required_length;
		stream->pos += required_length;
	}

	return status;
}


/** Create message parser */
APT_DECLARE(apt_message_parser_t*) apt_message_parser_create(void *obj, const apt_message_parser_vtable_t *vtable, apr_pool_t *pool)
{
	apt_message_parser_t *parser = apr_palloc(pool,sizeof(apt_message_parser_t));
	parser->obj = obj;
	parser->vtable = vtable;
	parser->pool = pool;
	parser->message = NULL;
	parser->content_length = 0;
	apt_string_reset(&parser->body);
	parser->stage = APT_MESSAGE_STAGE_START_LINE;
	parser->skip_lf = FALSE;
	return parser;
}

/** Parse message by raising corresponding event handlers */
APT_DECLARE(apt_message_status_e) apt_message_parser_run(apt_message_parser_t *parser, apt_text_stream_t *stream, void **message)
{
	apt_message_status_e status = APT_MESSAGE_STATUS_INCOMPLETE;
	if(parser->skip_lf == TRUE) {
		/* skip <LF> occurred as a result of message segmentation between <CR> and <LF> */
		apt_text_char_skip(stream,APT_TOKEN_LF);
		parser->skip_lf = FALSE;
	}

	do {
		if(parser->stage == APT_MESSAGE_STAGE_START_LINE) {
			apt_str_t start_line;
			/* read start line */
			if(apt_text_line_read(stream,&start_line) == FALSE) {
				status = APT_MESSAGE_STATUS_INVALID;
				break;
			}
			parser->message = parser->vtable->on_start_line(parser,&start_line,parser->pool);
			if(!parser->message) {
				status = APT_MESSAGE_STATUS_INVALID;
				break;
			}
			parser->stage = APT_MESSAGE_STAGE_HEADER;
		}

		if(parser->stage == APT_MESSAGE_STAGE_HEADER) {
			/* read header section */
			if(apt_header_section_parse(parser,stream) == FALSE) {
				break;
			}
			
			if(parser->content_length) {
				apt_str_t *body = &parser->body;
				body->buf = apr_palloc(parser->pool,parser->content_length+1);
				body->buf[parser->content_length] = '\0';
				body->length = 0;
				parser->stage = APT_MESSAGE_STAGE_BODY;
			}
			else {
				status = APT_MESSAGE_STATUS_COMPLETE;
				if(message) {
					*message = parser->message;
				}
				parser->stage = APT_MESSAGE_STAGE_START_LINE;
			}

			/* in the worst case message segmentation may occur between <CR> and <LF> 
			   of the final empty header */
			if(stream->pos == stream->end && *(stream->pos-1)== APT_TOKEN_CR) {
				/* if this is the case be prepared to skip <LF> */
				parser->skip_lf = TRUE;
				break;
			}
		}

		if(parser->stage == APT_MESSAGE_STAGE_BODY) {
			if(apt_message_body_read(parser,stream) == FALSE) {
				break;
			}
			
			parser->vtable->on_body(parser,parser->message,&parser->body);
			status = APT_MESSAGE_STATUS_COMPLETE;
			if(message) {
				*message = parser->message;
			}
			parser->stage = APT_MESSAGE_STAGE_START_LINE;
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	return status;
}

/** Get external object associated with parser */
APT_DECLARE(void*) apt_message_parser_object_get(apt_message_parser_t *parser)
{
	return parser->obj;
}


/** Create message generator */
APT_DECLARE(apt_message_generator_t*) apt_message_generator_create(void *obj, const apt_message_generator_vtable_t *vtable, apr_pool_t *pool)
{
	apt_message_generator_t *generator = apr_palloc(pool,sizeof(apt_message_generator_t));
	generator->obj = obj;
	generator->vtable = vtable;
	generator->pool = pool;
	generator->message = NULL;
	generator->header = NULL;
	generator->body = NULL;
	generator->content_length = 0;
	generator->stage = APT_MESSAGE_STAGE_START_LINE;
	return generator;
}

static apt_message_status_e apt_message_generator_break(apt_message_generator_t *generator, apt_text_stream_t *stream)
{
	/* failed to generate message */
	if(apt_text_is_eos(stream) == TRUE) {
		/* end of stream reached */
		return APT_MESSAGE_STATUS_INCOMPLETE;
	}

	/* error case */
	return APT_MESSAGE_STATUS_INVALID;
}

/** Generate message */
APT_DECLARE(apt_message_status_e) apt_message_generator_run(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream)
{
	if(!message) {
		return APT_MESSAGE_STATUS_INVALID;
	}

	if(message != generator->message) {
		generator->stage = APT_MESSAGE_STAGE_START_LINE;
		generator->message = message;
	}

	if(generator->stage == APT_MESSAGE_STAGE_START_LINE) {
		/* generate start-line */
		if(generator->vtable->initialize(generator,message,stream,&generator->header,&generator->body) == FALSE) {
			return apt_message_generator_break(generator,stream);
		}

		if(!generator->header || !generator->body) {
			return APT_MESSAGE_STATUS_INVALID;
		}

		/* generate header */
		if(apt_header_section_generate(generator->header,stream) == FALSE) {
			return apt_message_generator_break(generator,stream);
		}

		if(generator->vtable->finalize) {
			generator->vtable->finalize(generator,message,stream);
		}

		generator->stage = APT_MESSAGE_STAGE_START_LINE;
		generator->content_length = generator->body->length;
		if(generator->content_length) {
			generator->body->length = 0;
			generator->stage = APT_MESSAGE_STAGE_BODY;
		}
	}

	if(generator->stage == APT_MESSAGE_STAGE_BODY) {
		if(apt_message_body_write(generator,stream) == FALSE) {
			return apt_message_generator_break(generator,stream);
		}
		
		generator->stage = APT_MESSAGE_STAGE_START_LINE;
	}

	return APT_MESSAGE_STATUS_COMPLETE;
}

/** Get external object associated with generator */
APT_DECLARE(void*) apt_message_generator_object_get(apt_message_generator_t *generator)
{
	return generator->obj;
}
