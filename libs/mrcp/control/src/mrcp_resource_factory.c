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

#include <apr_hash.h>
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "mrcp_resource.h"
#include "mrcp_generic_header.h"

/** Resource factory definition (aggregation of resources) */
struct mrcp_resource_factory_t {
	/** Array of MRCP resources (reference by id) */
	mrcp_resource_t **resource_array;
	/** Number of MRCP resources */
	apr_size_t        resource_count;
	/** Hash of MRCP resources (reference by name) */
	apr_hash_t       *resource_hash;
};

/** Create MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_create(apr_size_t resource_count, apr_pool_t *pool)
{
	apr_size_t i;
	mrcp_resource_factory_t *resource_factory;
	if(resource_count == 0) {
		return NULL;
	}

	resource_factory = apr_palloc(pool,sizeof(mrcp_resource_factory_t));
	resource_factory->resource_count = resource_count;
	resource_factory->resource_array = apr_palloc(pool,sizeof(mrcp_resource_t*)*resource_count);
	for(i=0; i<resource_count; i++) {
		resource_factory->resource_array[i] = NULL;
	}
	resource_factory->resource_hash = apr_hash_make(pool);
	return resource_factory;
}

/** Destroy MRCP resource container */
MRCP_DECLARE(apt_bool_t) mrcp_resource_factory_destroy(mrcp_resource_factory_t *resource_factory)
{
	if(resource_factory->resource_array) {
		resource_factory->resource_array = NULL;
	}
	resource_factory->resource_count = 0;
	return TRUE;
}

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource)
{	
	if(!resource || resource->id >= resource_factory->resource_count) {
		/* invalid params */
		return FALSE;
	}
	if(resource_factory->resource_array[resource->id]) {
		/* resource with specified id already exists */
		return FALSE;
	}
	if(mrcp_resource_validate(resource) != TRUE) {
		/* invalid resource */
		return FALSE;
	}
	resource_factory->resource_array[resource->id] = resource;
	apr_hash_set(resource_factory->resource_hash,resource->name.buf,resource->name.length,resource);
	return TRUE;
}

/** Get MRCP resource by resource id */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_get(mrcp_resource_factory_t *resource_factory, mrcp_resource_id resource_id)
{
	if(resource_id >= resource_factory->resource_count) {
		return NULL;
	}
	return resource_factory->resource_array[resource_id];
}

/** Find MRCP resource by resource name */
MRCP_DECLARE(mrcp_resource_t*) mrcp_resource_find(mrcp_resource_factory_t *resource_factory, const apt_str_t *name)
{
	if(!name->buf || !name->length) {
		return NULL;
	}

	return apr_hash_get(resource_factory->resource_hash,name->buf,name->length);
}






/** Set header accessor interface */
static APR_INLINE void mrcp_generic_header_accessor_set(mrcp_message_t *message)
{
	message->header.generic_header_accessor.vtable = mrcp_generic_header_vtable_get(message->start_line.version);
}

/** Associate MRCP resource specific data by resource identifier */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_id(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;
	resource = mrcp_resource_get(resource_factory,message->channel_id.resource_id);
	if(!resource) {
		return FALSE;
	}
	/* associate resource_name and resource_id */
	message->channel_id.resource_name = resource->name;

	mrcp_generic_header_accessor_set(message);

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		const apt_str_t *name = apt_string_table_str_get(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			message->start_line.method_id);
		if(!name) {
			return FALSE;
		}
		message->start_line.method_name = *name;
	}

	message->header.resource_header_accessor.vtable = 
		resource->get_resource_header_vtable(message->start_line.version);

	return TRUE;
}

/** Associate MRCP resource specific data by resource name */
MRCP_DECLARE(apt_bool_t) mrcp_message_resourcify_by_name(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message)
{
	mrcp_resource_t *resource;
	/* associate resource_name and resource_id */
	resource = mrcp_resource_find(resource_factory,&message->channel_id.resource_name);
	if(!resource) {
		return FALSE;
	}
	message->channel_id.resource_id = resource->id;

	mrcp_generic_header_accessor_set(message);

	/* associate method_name and method_id */
	if(message->start_line.message_type == MRCP_MESSAGE_TYPE_REQUEST) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_method_str_table(message->start_line.version),
			resource->method_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->method_count) {
			return FALSE;
		}
	}
	else if(message->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) {
		message->start_line.method_id = apt_string_table_id_find(
			resource->get_event_str_table(message->start_line.version),
			resource->event_count,
			&message->start_line.method_name);
		if(message->start_line.method_id >= resource->event_count) {
			return FALSE;
		}
	}

	message->header.resource_header_accessor.vtable = 
		resource->get_resource_header_vtable(message->start_line.version);
	
	return TRUE;
}
