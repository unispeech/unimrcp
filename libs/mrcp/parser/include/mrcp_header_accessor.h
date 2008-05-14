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

#ifndef __MRCP_HEADER_ACCESSOR_H__
#define __MRCP_HEADER_ACCESSOR_H__

/**
 * @file mrcp_header_accessor.h
 * @brief Abstract MRCP Header Accessor
 */ 

#include "apt_string_table.h"
#include "apt_text_stream.h"
#include "mrcp.h"

APT_BEGIN_EXTERN_C

/** Bit field masks are used to define properties */
typedef int mrcp_header_property_t;

typedef struct mrcp_header_accessor_t mrcp_header_accessor_t;
typedef struct mrcp_header_vtable_t mrcp_header_vtable_t;

/** MRCP header accessor interface */
struct mrcp_header_vtable_t {
	/** Allocate actual header data */
	void* (*allocator)(mrcp_header_accessor_t *header, apr_pool_t *pool);
	/** Destroy header data */
	void (*destructor)(mrcp_header_accessor_t *header);

	/** Parse header field */
	apt_bool_t (*field_parser)(mrcp_header_accessor_t *header, apr_size_t id, const apt_str_t *value, apr_pool_t *pool);
	/** Generate header field */
	apt_bool_t (*field_generator)(mrcp_header_accessor_t *header, apr_size_t id, apt_str_t *value);
	/** Duplicate header field */
	apt_bool_t (*field_duplicator)(mrcp_header_accessor_t *header, const mrcp_header_accessor_t *src, apr_size_t id, apr_pool_t *pool);

	/** Table of fields  */
	const apt_str_table_item_t *field_table;
	/** Number of fields  */
	apr_size_t                  field_count;
};


/** Initialize header vtable */
static APR_INLINE void mrcp_header_vtable_init(mrcp_header_vtable_t *vtable)
{
	vtable->allocator = NULL;
	vtable->destructor = NULL;
	vtable->field_parser = NULL;
	vtable->field_generator = NULL;
	vtable->field_duplicator = NULL;
	vtable->field_table = NULL;
	vtable->field_count = 0;
}

/** Validate header vtable */
static APR_INLINE apt_bool_t mrcp_header_vtable_validate(const mrcp_header_vtable_t *vtable)
{
	return (vtable->allocator && vtable->destructor && 
		vtable->field_parser && vtable->field_generator &&
		vtable->field_duplicator && vtable->field_table && 
		vtable->field_count) ?	TRUE : FALSE;
}



/** MRCP header accessor */
struct mrcp_header_accessor_t {
	/** Actual header data allocated by accessor */
	void                       *data;
	/** Property set explicitly shows which fields are present(set) in entire header */
	mrcp_header_property_t      property_set;
	
	/** Header accessor interface */
	const mrcp_header_vtable_t *vtable;
};

/** Initialize header accessor */
static APR_INLINE void mrcp_header_accessor_init(mrcp_header_accessor_t *header)
{
	header->data = NULL;
	header->property_set = 0;
	header->vtable = NULL;
}


/** Allocate header data */
static APR_INLINE void* mrcp_header_allocate(mrcp_header_accessor_t *header, apr_pool_t *pool)
{
	if(header->data) {
		return header->data;
	}
	if(!header->vtable || !header->vtable->allocator) {
		return NULL;
	}
	return header->vtable->allocator(header,pool);
}

/** Destroy header data */
static APR_INLINE void mrcp_header_destroy(mrcp_header_accessor_t *header)
{
	if(!header->vtable || !header->vtable->destructor) {
		return;
	}
	header->vtable->destructor(header);
}


/** Parse header */
MRCP_DECLARE(apt_bool_t) mrcp_header_parse(mrcp_header_accessor_t *header, apt_name_value_t *name_value, apr_pool_t *pool);

/** Generate header */
MRCP_DECLARE(apt_bool_t) mrcp_header_generate(mrcp_header_accessor_t *header, apt_text_stream_t *text_stream);

/** Set header */
MRCP_DECLARE(apt_bool_t) mrcp_header_set(mrcp_header_accessor_t *header, const mrcp_header_accessor_t *src, mrcp_header_property_t mask, apr_pool_t *pool);

/** Inherit header */
MRCP_DECLARE(apt_bool_t) mrcp_header_inherit(mrcp_header_accessor_t *header, const mrcp_header_accessor_t *parent, apr_pool_t *pool);



/** Add property */
static APR_INLINE void mrcp_header_property_add(mrcp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	*property_set |= mask;
}

/** Remove property */
static APR_INLINE void mrcp_header_property_remove(mrcp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	*property_set &= ~mask;
}

/** Check the property */
static APR_INLINE apt_bool_t mrcp_header_property_check(mrcp_header_property_t *property_set, apr_size_t id)
{
	int mask = 1 << id;
	return ((*property_set & mask) == mask) ? TRUE : FALSE;
}


APT_END_EXTERN_C

#endif /*__MRCP_HEADER_ACCESSOR_H__*/
