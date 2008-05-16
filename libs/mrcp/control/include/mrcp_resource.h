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

#ifndef __MRCP_RESOURCE_H__
#define __MRCP_RESOURCE_H__

/**
 * @file mrcp_resource.h
 * @brief Abstract MRCP Resource
 */ 

#include "mrcp_types.h"
#include "mrcp_header_accessor.h"

APT_BEGIN_EXTERN_C


/** MRCP resource definition */
struct mrcp_resource_t {
	/** MRCP resource identifier */
	mrcp_resource_id            id;
	/** MRCP resource name */
	const apt_str_t            *name;

	/** MRCP methods registered in the resource */
	const apt_str_table_item_t *method_table;
	/** Number of MRCP methods registered in the resource */
	size_t                      method_count;

	/** MRCP events registered in the resource */
	const apt_str_table_item_t *event_table;
	/** Number of MRCP events registered in the resource */
	size_t                      event_count;

	/** MRCP resource header accessor interface */
	const mrcp_header_vtable_t *header_vtable;
};

/** Initialize MRCP resource */
static APR_INLINE void mrcp_resource_init(mrcp_resource_t *resource)
{
	resource->method_count = 0;
	resource->method_table = NULL;
	resource->event_count = 0;
	resource->event_table = NULL;
	resource->header_vtable = NULL;
}

/** Validate MRCP resource */
static APR_INLINE apt_bool_t mrcp_resource_validate(mrcp_resource_t *resource)
{
	return (resource->name && resource->method_table && resource->method_count && 
		resource->event_table && resource->event_count && 
		resource->header_vtable) ? TRUE : FALSE;
}

APT_END_EXTERN_C

#endif /*__MRCP_RESOURCE_H__*/
