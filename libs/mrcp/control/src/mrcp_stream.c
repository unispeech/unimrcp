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

#include "mrcp_stream.h"
#include "mrcp_message.h"
#include "mrcp_resource_factory.h"
#include "mrcp_resource.h"
#include "apt_log.h"


/** MRCP parser */
struct mrcp_parser_t {
	apt_message_parser_t          *base;
	const mrcp_resource_factory_t *resource_factory;
	mrcp_resource_t               *resource;
};

/** MRCP generator */
struct mrcp_generator_t {
	apt_message_generator_t       *base;
	const mrcp_resource_factory_t *resource_factory;
};

/** Create message and read start line */
static void* mrcp_parser_message_create(apt_message_parser_t *parser, apt_text_stream_t *stream, apr_pool_t *pool);
/** Header field handler */
static apt_bool_t mrcp_parser_on_header_field(apt_message_parser_t *parser, void *message, apt_header_field_t *header_field);
/** Header separator handler */
static apt_bool_t mrcp_parser_on_header_separator(apt_message_parser_t *parser, void *message, apr_size_t *content_length);
/** Body handler */
static apt_bool_t mrcp_parser_on_body(apt_message_parser_t *parser, void *message, apt_str_t *body);

static const apt_message_parser_vtable_t parser_vtable = {
	mrcp_parser_message_create,
	mrcp_parser_on_header_field,
	mrcp_parser_on_header_separator,
	mrcp_parser_on_body,
};

/** Initialize by generating message start line and return header section and body */
apt_bool_t mrcp_generator_message_initialize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream, apt_header_section_t **header, apt_str_t **body);
/** Finalize by setting overall message length in start line */
apt_bool_t mrcp_generator_message_finalize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream);

static const apt_message_generator_vtable_t generator_vtable = {
	mrcp_generator_message_initialize,
	mrcp_generator_message_finalize
};


/** Create MRCP stream parser */
MRCP_DECLARE(mrcp_parser_t*) mrcp_parser_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_parser_t *parser = apr_palloc(pool,sizeof(mrcp_parser_t));
	parser->base = apt_message_parser_create(parser,&parser_vtable,pool);
	parser->resource_factory = resource_factory;
	parser->resource = NULL;
	return parser;
}

/** Set resource by name to be used for parsing MRCPv1 messages */
MRCP_DECLARE(void) mrcp_parser_resource_set(mrcp_parser_t *mrcp_parser, const apt_str_t *resource_name)
{
	if(resource_name) {
		mrcp_parser->resource = mrcp_resource_find(mrcp_parser->resource_factory,resource_name);
	}
}

/** Parse MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_parser_run(mrcp_parser_t *parser, apt_text_stream_t *stream, mrcp_message_t **message)
{
	return apt_message_parser_run(parser->base,stream,(void**)message);
}

/** Create message and read start line */
static void* mrcp_parser_message_create(apt_message_parser_t *parser, apt_text_stream_t *stream, apr_pool_t *pool)
{
	mrcp_parser_t *mrcp_parser;
	mrcp_message_t *mrcp_message;
	apt_str_t start_line;
	/* read start line */
	if(apt_text_line_read(stream,&start_line) == FALSE) {
		return NULL;
	}

	/* create new MRCP message */
	mrcp_parser = apt_message_parser_object_get(parser);
	mrcp_message = mrcp_message_create(pool);
	/* parse start-line */
	if(mrcp_start_line_parse(&mrcp_message->start_line,&start_line,mrcp_message->pool) == FALSE) {
		return NULL;
	}

	if(mrcp_message->start_line.version == MRCP_VERSION_1) {
		if(!mrcp_parser->resource) {
			return NULL;
		}
		apt_string_copy(
			&mrcp_message->channel_id.resource_name,
			&mrcp_parser->resource->name,
			pool);

		if(mrcp_message_resource_set(mrcp_message,mrcp_parser->resource) == FALSE) {
			return FALSE;
		}
	}

	return mrcp_message;
}

/** Header field handler */
static apt_bool_t mrcp_parser_on_header_field(apt_message_parser_t *parser, void *message, apt_header_field_t *header_field)
{
	mrcp_message_t *mrcp_message = message;
	if(!mrcp_message->resource && mrcp_message->start_line.version == MRCP_VERSION_2) {
		if(mrcp_channel_id_parse(&mrcp_message->channel_id,header_field,mrcp_message->pool) == TRUE) {
			mrcp_resource_t *resource;
			mrcp_parser_t *mrcp_parser = apt_message_parser_object_get(parser);
			/* find resource */
			resource = mrcp_resource_find(mrcp_parser->resource_factory,&mrcp_message->channel_id.resource_name);
			if(!resource) {
				return FALSE;
			}

			if(mrcp_message_resource_set(mrcp_message,resource) == FALSE) {
				return FALSE;
			}
			return TRUE;
		}
	}

	return mrcp_message_header_field_add(&mrcp_message->header,header_field,mrcp_message->pool);
}

/** Header separator handler */
static apt_bool_t mrcp_parser_on_header_separator(apt_message_parser_t *parser, void *message, apr_size_t *content_length)
{
	mrcp_message_t *mrcp_message = message;
	if(mrcp_generic_header_property_check(mrcp_message,GENERIC_HEADER_CONTENT_LENGTH) == TRUE) {
		mrcp_generic_header_t *generic_header = mrcp_generic_header_get(message);
		if(generic_header && generic_header->content_length) {
			*content_length = generic_header->content_length;
		}
	}
	return TRUE;
}

/** Body handler */
static apt_bool_t mrcp_parser_on_body(apt_message_parser_t *parser, void *message, apt_str_t *body)
{
	mrcp_message_t *mrcp_message = message;
	mrcp_message->body = *body;
	return TRUE;
}



/** Create MRCP stream generator */
MRCP_DECLARE(mrcp_generator_t*) mrcp_generator_create(const mrcp_resource_factory_t *resource_factory, apr_pool_t *pool)
{
	mrcp_generator_t *generator = apr_palloc(pool,sizeof(mrcp_generator_t));
	generator->base = apt_message_generator_create(generator,&generator_vtable,pool);
	generator->resource_factory = resource_factory;
	return generator;
}

/** Generate MRCP stream */
MRCP_DECLARE(apt_message_status_e) mrcp_generator_run(mrcp_generator_t *generator, mrcp_message_t *message, apt_text_stream_t *stream)
{
	return apt_message_generator_run(generator->base,message,stream);
}

/** Initialize by generating message start line and return header section and body */
apt_bool_t mrcp_generator_message_initialize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream, apt_header_section_t **header, apt_str_t **body)
{
	mrcp_message_t *mrcp_message = message;
	/* validate message */
	if(mrcp_message_validate(mrcp_message) == FALSE) {
		return FALSE;
	}
	/* generate start-line */
	if(mrcp_start_line_generate(&mrcp_message->start_line,stream) == FALSE) {
		return FALSE;
	}
		
	if(mrcp_message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&mrcp_message->channel_id,stream);
	}

	if(header) {
		*header = &mrcp_message->header.header_section;
	}
	if(body) {
		*body = &mrcp_message->body;
	}
	return TRUE;
}

/** Finalize by setting overall message length in start line */
apt_bool_t mrcp_generator_message_finalize(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream)
{
	mrcp_message_t *mrcp_message = message;
	/* finalize start-line generation */
	if(mrcp_start_line_finalize(&mrcp_message->start_line,mrcp_message->body.length,stream) == FALSE) {
		return FALSE;
	}
	return TRUE;
}

/** Generate MRCP message (excluding message body) */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(const mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *stream)
{
	apt_header_field_t *header_field;
	/* validate message */
	if(mrcp_message_validate(message) == FALSE) {
		return FALSE;
	}
	
	/* generate start-line */
	if(mrcp_start_line_generate(&message->start_line,stream) == FALSE) {
		return FALSE;
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&message->channel_id,stream);
	}

	/* generate header section */
	for(header_field = APR_RING_FIRST(&message->header.header_section.ring);
			header_field != APR_RING_SENTINEL(&message->header.header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {
		
		apt_header_field_generate(header_field,stream);
	}
	apt_text_eol_insert(stream);

	/* finalize start-line generation */
	if(mrcp_start_line_finalize(&message->start_line,message->body.length,stream) == FALSE) {
		return FALSE;
	}

	return TRUE;
}
