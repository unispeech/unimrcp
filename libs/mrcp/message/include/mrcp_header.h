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

#ifndef MRCP_HEADER_H
#define MRCP_HEADER_H

/**
 * @file mrcp_header.h
 * @brief MRCP Message Header Definition
 */ 

#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C

/** MRCP message header declaration */
typedef struct mrcp_message_header_t mrcp_message_header_t;

/** MRCP message-header */
struct mrcp_message_header_t {
	/** MRCP generic-header */
	mrcp_header_accessor_t generic_header_accessor;
	/** MRCP resource specific header */
	mrcp_header_accessor_t resource_header_accessor;

	/** Header section (collection of header fields)*/
	apt_header_section_t   header_section;
};

/** Initialize MRCP message-header */
static APR_INLINE void mrcp_message_header_init(mrcp_message_header_t *header)
{
	mrcp_header_accessor_init(&header->generic_header_accessor);
	mrcp_header_accessor_init(&header->resource_header_accessor);
}

/** Destroy MRCP message-header */
static APR_INLINE void mrcp_message_header_destroy(mrcp_message_header_t *header)
{
	mrcp_header_destroy(&header->generic_header_accessor);
	mrcp_header_destroy(&header->resource_header_accessor);
}

/** Allocate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_allocate(
						mrcp_message_header_t *header,
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool);


/** Parse MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_parse(mrcp_message_header_t *header, apt_text_stream_t *stream, apr_pool_t *pool);

/** Generate MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_generate(mrcp_message_header_t *header, apt_text_stream_t *stream);

/** Set MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_set(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool);

/** Get MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_get(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool);

/** Inherit MRCP message-header */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_inherit(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool);


APT_END_EXTERN_C

#endif /* MRCP_HEADER_H */
