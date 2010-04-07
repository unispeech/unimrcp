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

#include "mrcp_header.h"
#include "mrcp_generic_header.h"
#include "apt_text_message.h"

#define MRCP_CHANNEL_ID         "Channel-Identifier"
#define MRCP_CHANNEL_ID_LENGTH  (sizeof(MRCP_CHANNEL_ID)-1)


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

/** Add MRCP header field */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_add(mrcp_message_header_t *header, apt_header_field_t *header_field, apr_pool_t *pool)
{
	apt_bool_t status = FALSE;
	if(apt_string_is_empty(&header_field->name) == FALSE) {
		/* normal header */
		if(mrcp_header_field_value_parse(&header->resource_header_accessor,header_field,pool) == TRUE) {
			header_field->id += GENERIC_HEADER_COUNT;
			status = apt_header_section_field_add(&header->header_section,header_field);
		}
		else if(mrcp_header_field_value_parse(&header->generic_header_accessor,header_field,pool) == TRUE) {
			status = apt_header_section_field_add(&header->header_section,header_field);
		}
		else { 
			/* unknown MRCP header */
		}
	}
	return status;
}

/** Set (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_set(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(src_header_field = APR_RING_FIRST(&src_header->header_section.ring);
			src_header_field != APR_RING_SENTINEL(&src_header->header_section.ring, apt_header_field_t, link);
				src_header_field = APR_RING_NEXT(src_header_field, link)) {

		header_field = apt_header_field_copy(src_header_field,pool);
		if(header_field->id < GENERIC_HEADER_COUNT) {
			if(mrcp_header_field_value_duplicate(
					&header->generic_header_accessor,
					&src_header->generic_header_accessor,
					header_field->id,
					&header_field->value,
					pool) == TRUE) {
				apt_header_section_field_add(&header->header_section,header_field);
			}
		}
		else {
			if(mrcp_header_field_value_duplicate(
					&header->resource_header_accessor,
					&src_header->resource_header_accessor,
					header_field->id - GENERIC_HEADER_COUNT,
					&header_field->value,
					pool) == TRUE) {
				apt_header_section_field_add(&header->header_section,header_field);
			}
		}
	}

	return TRUE;
}

/** Get (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_get(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(header_field = APR_RING_FIRST(&header->header_section.ring);
			header_field != APR_RING_SENTINEL(&header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {

		src_header_field = apt_header_section_field_get(&src_header->header_section,header_field->id);
		if(src_header_field) {
			if(header_field->id < GENERIC_HEADER_COUNT) {
				apt_string_copy(&header_field->value,&src_header_field->value,pool);
				mrcp_header_field_value_duplicate(
						&header->generic_header_accessor,
						&src_header->generic_header_accessor,
						header_field->id,
						&header_field->value,
						pool);
			}
			else {
				apt_string_copy(&header_field->value,&src_header_field->value,pool);
				mrcp_header_field_value_duplicate(
						&header->resource_header_accessor,
						&src_header->resource_header_accessor,
						header_field->id - GENERIC_HEADER_COUNT,
						&header_field->value,
						pool);
			}
		}
	}

	return TRUE;
}

/** Inherit (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_inherit(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
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
				if(mrcp_header_field_value_duplicate(
						&header->generic_header_accessor,
						&src_header->generic_header_accessor,
						header_field->id,
						&header_field->value,
						pool) == TRUE) {
					apt_header_section_field_add(&header->header_section,header_field);
				}
			}
			else {
				if(mrcp_header_field_value_duplicate(
						&header->resource_header_accessor,
						&src_header->resource_header_accessor,
						header_field->id - GENERIC_HEADER_COUNT,
						&header_field->value,
						pool) == TRUE) {
					apt_header_section_field_add(&header->header_section,header_field);
				}
			}
		}
	}
	return TRUE;
}


/** Initialize MRCP channel-identifier */
MRCP_DECLARE(void) mrcp_channel_id_init(mrcp_channel_id *channel_id)
{
	apt_string_reset(&channel_id->session_id);
	apt_string_reset(&channel_id->resource_name);
}

/** Parse MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_parse(mrcp_channel_id *channel_id, const apt_header_field_t *header_field, apr_pool_t *pool)
{
	apt_bool_t match = FALSE;
	if(header_field->name.length) {
		if(header_field->value.length && strncasecmp(header_field->name.buf,MRCP_CHANNEL_ID,MRCP_CHANNEL_ID_LENGTH) == 0) {
			match = TRUE;
			apt_id_resource_parse(&header_field->value,'@',&channel_id->session_id,&channel_id->resource_name,pool);
		}
	}
	return match;
}

/** Generate MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_generate(mrcp_channel_id *channel_id, apt_text_stream_t *stream)
{
	apt_str_t *str;
	char *pos = stream->pos;

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

	stream->pos = pos;
	apt_text_eol_insert(stream);
	return TRUE;
}
