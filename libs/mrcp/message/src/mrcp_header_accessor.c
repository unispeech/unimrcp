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

MRCP_DECLARE(apt_bool_t) mrcp_header_parse(mrcp_header_accessor_t *accessor, apt_str_t *name, apt_text_stream_t *value, apr_pool_t *pool)
{
	size_t id;
	if(!accessor->vtable) {
		return FALSE;
	}

	id = apt_string_table_id_find(accessor->vtable->field_table,accessor->vtable->field_count,name);
	if(id >= accessor->vtable->field_count) {
		return FALSE;
	}

	if(accessor->vtable->parse_field(accessor,id,value,pool) == FALSE) {
		return FALSE;
	}
	
	mrcp_header_property_add(&accessor->property_set,id);
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_generate(mrcp_header_accessor_t *accessor, apt_text_stream_t *text_stream)
{
	const apt_str_t *name;
	apr_size_t i;
	mrcp_header_property_t property_set;

	if(!accessor->vtable) {
		return FALSE;
	}

	property_set = accessor->property_set;
	for(i=0; i<accessor->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			name = apt_string_table_str_get(accessor->vtable->field_table,accessor->vtable->field_count,i);
			if(!name) {
				continue;
			}
			
			apt_name_value_name_generate(name,text_stream);
			accessor->vtable->generate_field(accessor,i,text_stream);
			apt_text_eol_insert(text_stream);
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_set(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, mrcp_header_property_t mask, apr_pool_t *pool)
{
	size_t i;
	mrcp_header_property_t property_set = src->property_set;

	if(!accessor->vtable || !src->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(accessor,pool);

	property_set = src->property_set;
	for(i=0; i<src->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			if(mrcp_header_property_check(&mask,i) == TRUE) {
				accessor->vtable->duplicate_field(accessor,src,i,pool);
				mrcp_header_property_add(&accessor->property_set,i);
			}
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}

MRCP_DECLARE(apt_bool_t) mrcp_header_inherit(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *parent, apr_pool_t *pool)
{
	size_t i;
	mrcp_header_property_t property_set;

	if(!accessor->vtable || !parent->vtable) {
		return FALSE;
	}

	mrcp_header_allocate(accessor,pool);

	property_set = parent->property_set;
	for(i=0; i<parent->vtable->field_count && property_set != 0; i++) {
		if(mrcp_header_property_check(&property_set,i) == TRUE) {
			if(mrcp_header_property_check(&accessor->property_set,i) != TRUE) {
				accessor->vtable->duplicate_field(accessor,parent,i,pool);
				mrcp_header_property_add(&accessor->property_set,i);
			}
			
			mrcp_header_property_remove(&property_set,i);
		}
	}
	return TRUE;
}
