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

#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_resource.h"
#include "apt_text_message.h"
#include "apt_log.h"

#define MRCP_CHANNEL_ID         "Channel-Identifier"
#define MRCP_CHANNEL_ID_LENGTH  (sizeof(MRCP_CHANNEL_ID)-1)

/** Initialize MRCP channel-identifier */
MRCP_DECLARE(void) mrcp_channel_id_init(mrcp_channel_id *channel_id)
{
	apt_string_reset(&channel_id->session_id);
	apt_string_reset(&channel_id->resource_name);
}

/** Parse MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_parse(mrcp_channel_id *channel_id, apt_text_stream_t *text_stream, apr_pool_t *pool)
{
	apt_bool_t match = FALSE;
	apt_pair_t pair;
	do {
		if(apt_text_header_read(text_stream,&pair) == TRUE) {
			if(pair.name.length) {
				if(pair.value.length && strncasecmp(pair.name.buf,MRCP_CHANNEL_ID,MRCP_CHANNEL_ID_LENGTH) == 0) {
					match = TRUE;
					apt_id_resource_parse(&pair.value,'@',&channel_id->session_id,&channel_id->resource_name,pool);
					break;
				}
				/* skip this header, expecting channel identifier first */
			}
			else {
				/* empty header */
				break;
			}
		}
	}
	while(apt_text_is_eos(text_stream) == FALSE);
	return match;
}

/** Generate MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_generate(mrcp_channel_id *channel_id, apt_text_stream_t *text_stream)
{
	apt_str_t *str;
	char *pos = text_stream->pos;

	memcpy(pos,MRCP_CHANNEL_ID,MRCP_CHANNEL_ID_LENGTH);
	pos += MRCP_CHANNEL_ID_LENGTH;
	*pos++ = ':';
	*pos++ = APT_TOKEN_SP;
	
	str = &channel_id->session_id;
	memcpy(pos,str->buf,str->length);
	pos += str->length;
	*pos++ = '@';

	str = &channel_id->resource_name;
	memcpy(pos,str->buf,str->length);
	pos += str->length;

	text_stream->pos = pos;
	apt_text_eol_insert(text_stream);
	return TRUE;
}

/** Parse MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_parse(mrcp_message_header_t *message_header, apt_text_stream_t *text_stream, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	apr_size_t id;
	apt_bool_t result = FALSE;

	do {
		header_field = apt_header_field_parse(text_stream,pool);
		if(header_field) {
			if(apt_string_is_empty(&header_field->name) == FALSE) {
				/* normal header */
				if(mrcp_header_field_parse(&message_header->resource_header_accessor,header_field,&id,pool) == TRUE) {
					header_field->id = id + GENERIC_HEADER_COUNT;
					apt_header_section_field_add(&message_header->header_section,header_field);
				}
				else if(mrcp_header_field_parse(&message_header->generic_header_accessor,header_field,&id,pool) == TRUE) {
					header_field->id = id;
					apt_header_section_field_add(&message_header->header_section,header_field);
				}
				else { 
					/* unknown MRCP header */
				}
			}
			else {
				/* empty header => exit */
				result = TRUE;
				break;
			}
		}
		else {
			/* malformed header => skip to the next one */
		}
	}
	while(apt_text_is_eos(text_stream) == FALSE);

	return result;
}

/** Generate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_generate(mrcp_message_header_t *message_header, apt_text_stream_t *text_stream)
{
	apt_header_field_t *header_field;
	for(header_field = APR_RING_FIRST(&message_header->header_section.ring);
			header_field != APR_RING_SENTINEL(&message_header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {
		
		apt_header_field_generate(header_field,text_stream);
	}

	apt_text_eol_insert(text_stream);
	return TRUE;
}

/** Set MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_set(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(src_header_field = APR_RING_FIRST(&src_header->header_section.ring);
			src_header_field != APR_RING_SENTINEL(&src_header->header_section.ring, apt_header_field_t, link);
				src_header_field = APR_RING_NEXT(src_header_field, link)) {

		header_field = apt_header_field_copy(src_header_field,pool);
		if(header_field->id < GENERIC_HEADER_COUNT) {
			if(mrcp_header_field_duplicate(&header->generic_header_accessor,&src_header->generic_header_accessor,header_field->id,pool) == TRUE) {
				apt_header_section_field_add(&header->header_section,header_field);
			}
		}
		else {
			if(mrcp_header_field_duplicate(&header->resource_header_accessor,&src_header->resource_header_accessor,header_field->id - GENERIC_HEADER_COUNT,pool) == TRUE) {
				apt_header_section_field_add(&header->header_section,header_field);
			}
		}
	}

	return TRUE;
}

/** Get MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_get(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(header_field = APR_RING_FIRST(&header->header_section.ring);
			header_field != APR_RING_SENTINEL(&header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {

		src_header_field = apt_header_section_field_get(&src_header->header_section,header_field->id);
		if(src_header_field) {
			if(header_field->id < GENERIC_HEADER_COUNT) {
				if(mrcp_header_field_duplicate(&header->generic_header_accessor,&src_header->generic_header_accessor,header_field->id,pool) == TRUE) {
					apt_string_copy(&header_field->value,&src_header_field->value,pool);
				}
			}
			else {
				if(mrcp_header_field_duplicate(&header->resource_header_accessor,&src_header->resource_header_accessor,header_field->id - GENERIC_HEADER_COUNT,pool) == TRUE) {
					apt_string_copy(&header_field->value,&src_header_field->value,pool);
				}
			}
		}
	}

	return TRUE;
}

/** Inherit MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_inherit(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(src_header_field = APR_RING_FIRST(&src_header->header_section.ring);
			src_header_field != APR_RING_SENTINEL(&src_header->header_section.ring, apt_header_field_t, link);
				src_header_field = APR_RING_NEXT(src_header_field, link)) {

		header_field = apt_header_section_field_get(&header->header_section,src_header_field->id);
		if(!header_field) {
			header_field = apt_header_field_copy(src_header_field,pool);
			if(header_field->id < GENERIC_HEADER_COUNT) {
				if(mrcp_header_field_duplicate(&header->generic_header_accessor,&src_header->generic_header_accessor,header_field->id,pool) == TRUE) {
					apt_header_section_field_add(&header->header_section,header_field);
				}
			}
			else {
				if(mrcp_header_field_duplicate(&header->resource_header_accessor,&src_header->resource_header_accessor,header_field->id - GENERIC_HEADER_COUNT,pool) == TRUE) {
					apt_header_section_field_add(&header->header_section,header_field);
				}
			}
		}
	}
	return TRUE;
}


/** Allocate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_allocate(
						mrcp_message_header_t *header,
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool)
{
	if(!generic_header_vtable || !resource_header_vtable) {
		return FALSE;
	}

	header->generic_header_accessor.data = NULL;
	header->generic_header_accessor.vtable = generic_header_vtable;
	
	header->resource_header_accessor.data = NULL;
	header->resource_header_accessor.vtable = resource_header_vtable;

	apt_header_section_init(
		&header->header_section,
		header->generic_header_accessor.vtable->field_count +
		header->resource_header_accessor.vtable->field_count,
		pool);

	mrcp_header_allocate(&header->generic_header_accessor,pool);
	mrcp_header_allocate(&header->resource_header_accessor,pool);
	return TRUE;
}


/** Associate MRCP resource specific data by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_message_resource_set_by_id(mrcp_message_t *message, mrcp_resource_t *resource)
{
	if(!resource) {
		return FALSE;
	}
	message->resource = resource;
	message->channel_id.resource_name = resource->name;
	mrcp_message_header_allocate(
		&message->header,
		mrcp_generic_header_vtable_get(message->start_line.version),
		resource->get_resource_header_vtable(message->start_line.version),
		message->pool);

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}

	return TRUE;
}

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_resource_set(mrcp_message_t *message, mrcp_resource_t *resource)
{
	if(!resource) {
		return FALSE;
	}
	message->resource = resource;
	mrcp_message_header_allocate(
		&message->header,
		mrcp_generic_header_vtable_get(message->start_line.version),
		resource->get_resource_header_vtable(message->start_line.version),
		message->pool);
	
	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->method_count) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->event_count) {
			return FALSE;
		}
	}

	return TRUE;
}

/** Create MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_message_create(apr_pool_t *pool)
{
	mrcp_message_t *message = apr_palloc(pool,sizeof(mrcp_message_t));
	mrcp_start_line_init(&message->start_line);
	mrcp_channel_id_init(&message->channel_id);
	mrcp_message_header_init(&message->header);
	apt_string_reset(&message->body);
	message->resource = NULL;
	message->pool = pool;
	return message;
}

/** Create MRCP request message */
MRCP_DECLARE(mrcp_message_t*) mrcp_request_create(mrcp_resource_t *resource, mrcp_version_e version, mrcp_method_id method_id, apr_pool_t *pool)
{
	mrcp_message_t *request_message = mrcp_message_create(pool);
	request_message->start_line.message_type = MRCP_MESSAGE_TYPE_REQUEST;
	request_message->start_line.version = version;
	request_message->start_line.method_id = method_id;
	mrcp_message_resource_set_by_id(request_message,resource);
	return request_message;
}

/** Create MRCP response message */
MRCP_DECLARE(mrcp_message_t*) mrcp_response_create(const mrcp_message_t *request_message, apr_pool_t *pool)
{
	mrcp_message_t *response_message = mrcp_message_create(pool);
	response_message->start_line.message_type = MRCP_MESSAGE_TYPE_RESPONSE;
	response_message->start_line.request_state = MRCP_REQUEST_STATE_COMPLETE;
	response_message->start_line.status_code = MRCP_STATUS_CODE_SUCCESS;
	if(request_message) {
		response_message->channel_id = request_message->channel_id;
		response_message->start_line.request_id = request_message->start_line.request_id;
		response_message->start_line.version = request_message->start_line.version;
		response_message->start_line.method_id = request_message->start_line.method_id;
		mrcp_message_resource_set_by_id(response_message,request_message->resource);
	}
	return response_message;
}

/** Create MRCP event message */
MRCP_DECLARE(mrcp_message_t*) mrcp_event_create(const mrcp_message_t *request_message, mrcp_method_id event_id, apr_pool_t *pool)
{
	mrcp_message_t *event_message = mrcp_message_create(pool);
	event_message->start_line.message_type = MRCP_MESSAGE_TYPE_EVENT;
	event_message->start_line.method_id = event_id;
	if(request_message) {
		event_message->channel_id = request_message->channel_id;
		event_message->start_line.request_id = request_message->start_line.request_id;
		event_message->start_line.version = request_message->start_line.version;
		mrcp_message_resource_set_by_id(event_message,request_message->resource);
	}
	return event_message;
}

/** Destroy MRCP message */
MRCP_DECLARE(void) mrcp_message_destroy(mrcp_message_t *message)
{
	apt_string_reset(&message->body);
	mrcp_message_header_destroy(&message->header);
}

/** Validate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_validate(mrcp_message_t *message)
{
	if(message->body.length) {
		/* content length must be specified */
		mrcp_generic_header_t *generic_header = mrcp_generic_header_prepare(message);
		if(!generic_header) {
			return FALSE;
		}
		if(mrcp_generic_header_property_check(message,GENERIC_HEADER_CONTENT_LENGTH) != TRUE ||
		  !generic_header->content_length) {
			generic_header->content_length = message->body.length;
			mrcp_generic_header_property_add(message,GENERIC_HEADER_CONTENT_LENGTH);
		}
	}

	return TRUE;
}
