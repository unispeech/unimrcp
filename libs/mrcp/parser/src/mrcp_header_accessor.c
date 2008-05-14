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

#include "mrcp_header_accessor.h"

MRCP_DECLARE(apt_bool_t) mrcp_header_parse(mrcp_header_accessor_t *header, apt_name_value_t *name_value, apr_pool_t *pool)
{
	size_t id;
	if(!header->vtable) {
		return FALSE;
	}

	id = apt_string_table_id_find(header->vtable->field_table,header->vtable->field_count,&name_value->name);
	if(id >= header->vtable->field_count) {
		return FALSE;
	}

	if(header->vtable->field_parser(header,id,&name_value->value,pool) == FALSE) {
		return FALSE;
	}
	
	mrcp_header_property_add(&header->property_set,id);
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_generate(mrcp_header_accessor_t *header, apt_text_stream_t *text_stream)
{
	const apt_str_t *name;
	apt_str_t line;
	apr_size_t i;
	mrcp_header_property_t property_set;

	if(!header->vtable) {
		return FALSE;
	}

	property_set = header->property_set;
	for(i=0; i<header->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			name = apt_string_table_str_get(header->vtable->field_table,header->vtable->field_count,i);
			if(!name) {
				continue;
			}
			
			apt_name_value_name_generate(text_stream,name);
			text_stream->pos += header->vtable->field_generator(header,i,&line);
			apt_text_eol_insert(text_stream);
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_set(mrcp_header_accessor_t *header, const mrcp_header_accessor_t *src, mrcp_header_property_t mask, apr_pool_t *pool)
{
	size_t i;
	mrcp_header_property_t property_set = src->property_set;

	if(!header->vtable || !src->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(header,pool);

	property_set = src->property_set;
	for(i=0; i<src->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			if(mrcp_header_property_check(&mask,i) == TRUE) {
				header->vtable->field_duplicator(header,src,i,pool);
				mrcp_header_property_add(&header->property_set,i);
			}
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_inherit(mrcp_header_accessor_t *header, const mrcp_header_accessor_t *parent, apr_pool_t *pool)
{
	size_t i;
	mrcp_header_property_t property_set;

	if(!header->vtable || !parent->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(header,pool);

	property_set = parent->property_set;
	for(i=0; i<parent->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			if(mrcp_header_property_check(&header->property_set,i) != TRUE) {
				header->vtable->field_duplicator(header,parent,i,pool);
				mrcp_header_property_add(&header->property_set,i);
			}
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}
