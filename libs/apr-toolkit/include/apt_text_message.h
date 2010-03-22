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

#ifndef APT_TEXT_MESSAGE_H
#define APT_TEXT_MESSAGE_H

/**
 * @file apt_text_message.h
 * @brief Text Message Interface (RFC5322)
 */ 

#include "apt_header_field.h"
#include "apt_text_stream.h"

APT_BEGIN_EXTERN_C

/** Status of text message processing (parsing/generation) */
typedef enum {
	APT_MESSAGE_STATUS_COMPLETE,
	APT_MESSAGE_STATUS_INCOMPLETE,
	APT_MESSAGE_STATUS_INVALID
} apt_message_status_e;


/** Opaque text message parser */
typedef struct apt_message_parser_t apt_message_parser_t;
/** Vtable of text message parser */
typedef struct apt_message_parser_vtable_t apt_message_parser_vtable_t;

/** Opaque text message generator */
typedef struct apt_message_generator_t apt_message_generator_t;
/** Vtable of text message generator */
typedef struct apt_message_generator_vtable_t apt_message_generator_vtable_t;


/** Create message parser */
APT_DECLARE(apt_message_parser_t*) apt_message_parser_create(void *obj, const apt_message_parser_vtable_t *vtable, apr_pool_t *pool);

/** Parse message by raising corresponding event handlers */
APT_DECLARE(apt_message_status_e) apt_message_parser_run(apt_message_parser_t *parser, apt_text_stream_t *stream, void **message);


/** Create message generator */
APT_DECLARE(apt_message_generator_t*) apt_message_generator_create(void *obj, const apt_message_generator_vtable_t *vtable, apr_pool_t *pool);

/** Generate message */
APT_DECLARE(apt_message_status_e) apt_message_generator_run(apt_message_generator_t *generator, void *message, apt_text_stream_t *stream);


/** Vtable of text message parser */
struct apt_message_parser_vtable_t {
	/** Start line handler */
	void* (*on_start_line)(apt_str_t *start_line, apr_pool_t *pool);
	/** Header field handler */
	apt_bool_t (*on_header_field)(void *message, apt_header_field_t *header_field);
	/** Header separator handler */
	apt_bool_t (*on_header_separator)(void *message, apr_size_t *content_length);
	/** Body handler */
	apt_bool_t (*on_body)(void *message, apt_str_t *body);
};

/** Vtable of text message generator */
struct apt_message_generator_vtable_t {
	/** Initialize by generating message start line and return header section and body */
	apt_bool_t (*initialize)(void *message, apt_text_stream_t *stream, apt_header_section_t **header, apt_str_t **body);
	/** Finalize message start-line and header generation */
	apt_bool_t (*finalize)(void *message, apt_text_stream_t *stream);
};


APT_END_EXTERN_C

#endif /* APT_TEXT_MESSAGE_H */
