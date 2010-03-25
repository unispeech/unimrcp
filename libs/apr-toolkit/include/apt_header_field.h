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

#ifndef APT_HEADER_FIELD_H
#define APT_HEADER_FIELD_H

/**
 * @file apt_header_field.h
 * @brief Header Field Declaration (RFC5322)
 */ 

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** Header field declaration */
typedef struct apt_header_field_t apt_header_field_t;
/** Header section declaration */
typedef struct apt_header_section_t apt_header_section_t;

/** Header field */
struct apt_header_field_t {
	/** Ring entry */
	APR_RING_ENTRY(apt_header_field_t) link;

	/** Name of the header field */
	apt_str_t name;
	/** Value of the header field */
	apt_str_t value;

	/** Numeric identifier associated with name */
	int       id;
};

/** 
 * Header section 
 * @remark The header section is a collection of header fields. 
 * The header fields are stored in both a ring and an array.
 * The goal is to ensure efficient access and manipulation on the header fields.
 */
struct apt_header_section_t {
	/** List of header fields (name-value pairs) */
	APR_RING_HEAD(apt_head_t, apt_header_field_t) ring;
	/** Array of pointers to header fields */
	apt_header_field_t **arr;
	/** Max number of header fields */
	int                  arr_size;
};


/**
 * Allocate an empty header field.
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_alloc(apr_pool_t *pool);

/**
 * Copy specified header field.
 * @param header_field the header field to copy
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_header_field_t*) apt_header_field_copy(const apt_header_field_t *src_header_field, apr_pool_t *pool);

/**
 * Initialize header section (collection of header fields).
 * @param header the header section to initialize
 * @param max_field_count the number of max header fields in a section (protocol dependent)
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(void) apt_header_section_init(apt_header_section_t *header, int max_field_count, apr_pool_t *pool);

/**
 * Add header field to header section.
 * @param header the header section to add field to
 * @param header_field the header field to add
 * @param id the identifier associated with the header_field
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_add(apt_header_section_t *header, apt_header_field_t *header_field);

/**
 * Remove header field from header section.
 * @param header the header section to remove field from
 * @param id the identifier associated with the header_field to use
 */
APT_DECLARE(apt_bool_t) apt_header_section_field_remove(apt_header_section_t *header, int id);

/**
 * Check whether specified header field is set
 * @param header the header section to use
 * @param id the identifier associated with the header_field to check
 */
static APR_INLINE apt_bool_t apt_header_section_field_check(const apt_header_section_t *header, int id)
{
	if(id < header->arr_size) {
		return header->arr[id] ? TRUE : FALSE;
	}
	return FALSE;
}

/**
 * Get header field by specified identifier
 * @param header the header section to use
 * @param id the identifier associated with the header_field
 */
static APR_INLINE apt_header_field_t* apt_header_section_field_get(const apt_header_section_t *header, int id)
{
	if(id < header->arr_size) {
		return header->arr[id];
	}
	return NULL;
}

APT_END_EXTERN_C

#endif /* APT_HEADER_FIELD_H */
