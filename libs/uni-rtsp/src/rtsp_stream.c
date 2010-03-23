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

#include "rtsp_stream.h"
#include "apt_log.h"

/** RTSP parser */
struct rtsp_parser_t {
	apt_message_parser_t    *base;
};

/** RTSP generator */
struct rtsp_generator_t {
	apt_message_generator_t *base;
};

/** Start line handler */
static void* rtsp_parser_on_start_line(apt_message_parser_t *parser, apt_str_t *start_line, apr_pool_t *pool);
/** Header field handler */
static apt_bool_t rtsp_parser_on_header_field(apt_message_parser_t *parser, void *message, apt_header_field_t *header_field);
/** Header separator handler */
static apt_bool_t rtsp_parser_on_header_separator(apt_message_parser_t *parser, void *message, apr_size_t *content_length);
/** Body handler */
static apt_bool_t rtsp_parser_on_body(apt_message_parser_t *parser, void *message, apt_str_t *body);

static const apt_message_parser_vtable_t parser_vtable = {
	rtsp_parser_on_start_line,
	rtsp_parser_on_header_field,
	rtsp_parser_on_header_separator,
	rtsp_parser_on_body,
};


/** Initialize by generating message start line and return header section and body */
apt_bool_t rtsp_generator_message_initialize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream, apt_header_section_t **header, apt_str_t **body);

static const apt_message_generator_vtable_t generator_vtable = {
	rtsp_generator_message_initialize,
	NULL
};


/** Create RTSP parser */
RTSP_DECLARE(rtsp_parser_t*) rtsp_parser_create(apr_pool_t *pool)
{
	rtsp_parser_t *parser = apr_palloc(pool,sizeof(rtsp_parser_t));
	parser->base = apt_message_parser_create(parser,&parser_vtable,pool);
	return parser;
}

/** Parse RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_parser_run(rtsp_parser_t *parser, apt_text_stream_t *stream, rtsp_message_t **message)
{
	return apt_message_parser_run(parser->base,stream,(void**)message);
}

/** Start line handler */
static void* rtsp_parser_on_start_line(apt_message_parser_t *parser, apt_str_t *start_line, apr_pool_t *pool)
{
	rtsp_message_t *message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,pool);
	if(rtsp_start_line_parse(&message->start_line,start_line,message->pool) == FALSE) {
		return NULL;
	}
	
	return message;
}

/** Header field handler */
static apt_bool_t rtsp_parser_on_header_field(apt_message_parser_t *parser, void *message, apt_header_field_t *header_field)
{
	rtsp_message_t *rtsp_message = message;
	return rtsp_header_field_add(&rtsp_message->header,header_field,rtsp_message->pool);
}

/** Header separator handler */
static apt_bool_t rtsp_parser_on_header_separator(apt_message_parser_t *parser, void *message, apr_size_t *content_length)
{
	rtsp_message_t *rtsp_message = message;
	if(rtsp_header_property_check(&rtsp_message->header,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE) {
		*content_length = rtsp_message->header.content_length;
	}

	return TRUE;
}

/** Body handler */
static apt_bool_t rtsp_parser_on_body(apt_message_parser_t *parser, void *message, apt_str_t *body)
{
	rtsp_message_t *rtsp_message = message;
	rtsp_message->body = *body;
	return TRUE;
}


/** Create RTSP stream generator */
RTSP_DECLARE(rtsp_generator_t*) rtsp_generator_create(apr_pool_t *pool)
{
	rtsp_generator_t *generator = apr_palloc(pool,sizeof(rtsp_generator_t));
	generator->base = apt_message_generator_create(generator,&generator_vtable,pool);
	return generator;
}


/** Generate RTSP stream */
RTSP_DECLARE(apt_message_status_e) rtsp_generator_run(rtsp_generator_t *generator, rtsp_message_t *message, apt_text_stream_t *stream)
{
	return apt_message_generator_run(generator->base,message,stream);
}

/** Initialize by generating message start line and return header section and body */
apt_bool_t rtsp_generator_message_initialize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream, apt_header_section_t **header, apt_str_t **body)
{
	rtsp_message_t *rtsp_message = message;
	if(header) {
		*header = &rtsp_message->header.header_section;
	}
	if(body) {
		*body = &rtsp_message->body;
	}
	return rtsp_start_line_generate(&rtsp_message->start_line,stream);
}
