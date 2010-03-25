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

/** Parse MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_parse(mrcp_message_header_t *header, apt_text_stream_t *stream, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	apr_size_t id;
	apt_bool_t result = FALSE;

	do {
		header_field = apt_header_field_parse(stream,pool);
		if(header_field) {
			if(apt_string_is_empty(&header_field->name) == FALSE) {
				/* normal header */
				if(mrcp_header_field_parse(&header->resource_header_accessor,header_field,&id,pool) == TRUE) {
					header_field->id = id + GENERIC_HEADER_COUNT;
					apt_header_section_field_add(&header->header_section,header_field);
				}
				else if(mrcp_header_field_parse(&header->generic_header_accessor,header_field,&id,pool) == TRUE) {
					header_field->id = id;
					apt_header_section_field_add(&header->header_section,header_field);
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
	while(apt_text_is_eos(stream) == FALSE);

	return result;
}

/** Generate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_generate(mrcp_message_header_t *header, apt_text_stream_t *stream)
{
	apt_header_field_t *header_field;
	for(header_field = APR_RING_FIRST(&header->header_section.ring);
			header_field != APR_RING_SENTINEL(&header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {
		
		apt_header_field_generate(header_field,stream);
	}

	apt_text_eol_insert(stream);
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
