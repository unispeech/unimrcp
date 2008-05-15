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

#include "mrcp_parser.h"
#include "mrcp_generic_header.h"


/** Validate MRCP message */
static apt_bool_t mrcp_message_validate(mrcp_message_t *message)
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

/** Associate MRCP resource specific data by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_message_associate_resource_by_id(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;

	resource = mrcp_resource_get(resource_factory,message->channel_id.resource_id);
	if(!resource) {
		return FALSE;
	}

	/* associate resource_name and resource_id */
	message->channel_id.resource_name = *resource->name;

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
											resource->method_table,
											resource->method_count,
											message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
											resource->event_table,
											resource->event_count,
											message->start_line.method_id);
		message->start_line.method_name = *name;
	}

	/* set header accessors for the entire message */
	message->header.resource_header_accessor.vtable = resource->header_vtable;
	message->header.generic_header_accessor.vtable = resource_factory->header_vtable;
	return TRUE;
}

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_associate_resource_by_name(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;
	const apt_str_t *name;

	/* associate resource_name and resource_id */
	name = &message->channel_id.resource_name;
	message->channel_id.resource_id = mrcp_resource_id_find(resource_factory,name);
	resource = mrcp_resource_get(resource_factory,message->channel_id.resource_id);
	if(!resource) {
		return FALSE;
	}

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
											resource->method_table,
											resource->method_count,
											&message->start_line.method_name);
		if(message->start_line.method_id >= resource->method_count) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
											resource->event_table,
											resource->event_count,
											&message->start_line.method_name);
		if(message->start_line.method_id >= resource->event_count) {
			return FALSE;
		}
	}

	/* sets header accessors for the entire message */
	message->header.resource_header_accessor.vtable = resource->header_vtable;
	message->header.generic_header_accessor.vtable = resource_factory->header_vtable;
	return TRUE;
}


/** Parse MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_parse(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *text_stream)
{
	if(mrcp_start_line_parse(&message->start_line,text_stream,message->pool) == FALSE) {
		return FALSE;
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_parse(&message->channel_id,text_stream,message->pool);
	}

	if(mrcp_message_associate_resource_by_name(resource_factory,message) == FALSE) {
		return FALSE;
	}

	if(mrcp_message_header_parse(&message->header,text_stream,message->pool) == FALSE) {
		return FALSE;
	}

	mrcp_body_parse(message,text_stream,message->pool);

	return TRUE;
}

/** Generate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *text_stream)
{
	if(mrcp_message_associate_resource_by_id(resource_factory,message) == FALSE) {
		return FALSE;
	}

	if(mrcp_message_validate(message) == FALSE) {
		return FALSE;
	}
	
	if(mrcp_start_line_generate(&message->start_line,text_stream) == FALSE) {
		return FALSE;
	}

	if(message->start_line.version == MRCP_VERSION_2) {
		mrcp_channel_id_generate(&message->channel_id,text_stream);
	}

	if(mrcp_message_header_generate(&message->header,text_stream) == FALSE) {
		return FALSE;
	}

	mrcp_body_generate(message,text_stream);
	
	text_stream->text.length = text_stream->pos - text_stream->text.buf;
	mrcp_start_line_finalize(&message->start_line,text_stream);
	return TRUE;
}
