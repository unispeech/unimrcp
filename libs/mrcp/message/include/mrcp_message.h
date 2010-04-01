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

#ifndef MRCP_MESSAGE_H
#define MRCP_MESSAGE_H

/**
 * @file mrcp_message.h
 * @brief MRCP Message Definition
 */ 

#include "mrcp_types.h"
#include "mrcp_start_line.h"
#include "mrcp_header.h"
#include "mrcp_generic_header.h"

APT_BEGIN_EXTERN_C


/** MRCP message */
struct mrcp_message_t {
	/** Start-line of MRCP message */
	mrcp_start_line_t      start_line;
	/** Channel-identifier of MRCP message */
	mrcp_channel_id        channel_id;
	/** Header of MRCP message */
	mrcp_message_header_t  header;
	/** Body of MRCP message */
	apt_str_t              body;

	/** Associated MRCP resource */
	const mrcp_resource_t *resource;
	/** Memory pool MRCP message is allocated from */
	apr_pool_t            *pool;
};

/** Create MRCP message */
MRCP_DECLARE(mrcp_message_t*) mrcp_message_create(apr_pool_t *pool);

/** Create MRCP request message */
MRCP_DECLARE(mrcp_message_t*) mrcp_request_create(const mrcp_resource_t *resource, mrcp_version_e version, mrcp_method_id method_id, apr_pool_t *pool);
/** Create MRCP response message */
MRCP_DECLARE(mrcp_message_t*) mrcp_response_create(const mrcp_message_t *request_message, apr_pool_t *pool);
/** Create MRCP event message */
MRCP_DECLARE(mrcp_message_t*) mrcp_event_create(const mrcp_message_t *request_message, mrcp_method_id event_id, apr_pool_t *pool);

/** Associate MRCP resource with message */
MRCP_DECLARE(apt_bool_t) mrcp_message_resource_set(mrcp_message_t *message, const mrcp_resource_t *resource);

/** Validate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_validate(mrcp_message_t *message);

/** Destroy MRCP message */
MRCP_DECLARE(void) mrcp_message_destroy(mrcp_message_t *message);



/** Get MRCP generic-header */
static APR_INLINE void* mrcp_generic_header_get(mrcp_message_t *mrcp_message)
{
	return mrcp_message->header.generic_header_accessor.data;
}

/** Prepare MRCP generic-header */
static APR_INLINE void* mrcp_generic_header_prepare(mrcp_message_t *mrcp_message)
{
	return mrcp_header_allocate(&mrcp_message->header.generic_header_accessor,mrcp_message->pool);
}

/** Add MRCP generic-header property */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_property_add(mrcp_message_t *mrcp_message, apr_size_t id);

/** Add MRCP generic-header name only property (should be used to construct empty header fiedls for GET-PARAMS request) */
MRCP_DECLARE(apt_bool_t) mrcp_generic_header_name_property_add(mrcp_message_t *mrcp_message, apr_size_t id);

/** Remove MRCP generic-header property */
static APR_INLINE apt_bool_t mrcp_generic_header_property_remove(mrcp_message_t *mrcp_message, apr_size_t id)
{
	return apt_header_section_field_remove(&mrcp_message->header.header_section,id);
}

/** Check MRCP generic-header property */
static APR_INLINE apt_bool_t mrcp_generic_header_property_check(mrcp_message_t *mrcp_message, apr_size_t id)
{
	return apt_header_section_field_check(&mrcp_message->header.header_section,id);
}


/** Get MRCP resource-header */
static APR_INLINE void* mrcp_resource_header_get(const mrcp_message_t *mrcp_message)
{
	return mrcp_message->header.resource_header_accessor.data;
}

/** Prepare MRCP resource-header */
static APR_INLINE void* mrcp_resource_header_prepare(mrcp_message_t *mrcp_message)
{
	return mrcp_header_allocate(&mrcp_message->header.resource_header_accessor,mrcp_message->pool);
}

/** Add MRCP resource-header property */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_property_add(mrcp_message_t *mrcp_message, apr_size_t id);

/** Add MRCP resource-header name only property (should be used to construct empty header fields for GET-PARAMS request) */
MRCP_DECLARE(apt_bool_t) mrcp_resource_header_name_property_add(mrcp_message_t *mrcp_message, apr_size_t id);

/** Remove MRCP resource-header property */
static APR_INLINE apt_bool_t mrcp_resource_header_property_remove(mrcp_message_t *mrcp_message, apr_size_t id)
{
	return apt_header_section_field_remove(&mrcp_message->header.header_section,id + GENERIC_HEADER_COUNT);
}

/** Check MRCP resource-header property */
static APR_INLINE apt_bool_t mrcp_resource_header_property_check(mrcp_message_t *mrcp_message, apr_size_t id)
{
	return apt_header_section_field_check(&mrcp_message->header.header_section,id + GENERIC_HEADER_COUNT);
}

/** Add MRCP header field */
static APR_INLINE apt_bool_t mrcp_message_header_field_add(mrcp_message_t *message, apt_header_field_t *header_field)
{
	return mrcp_header_field_add(&message->header,header_field,message->pool);
}


APT_END_EXTERN_C

#endif /* MRCP_MESSAGE_H */
