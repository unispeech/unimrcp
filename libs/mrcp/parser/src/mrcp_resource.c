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

#include "mrcp_resource.h"
#include "mrcp_generic_header.h"

/** Create MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_resource_factory_create(apr_size_t resource_count, const apt_str_table_item_t *string_table, apr_pool_t *pool)
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
	
	resource_factory->string_table = string_table;

	resource_factory->header_vtable = mrcp_generic_header_vtable_get();
	return resource_factory;
}

/** Destroy MRCP resource container */
MRCP_DECLARE(apt_bool_t) mrcp_resource_factory_destroy(mrcp_resource_factory_t *resource_factory)
{
	if(resource_factory->resource_array) {
		resource_factory->resource_array = NULL;
	}
	resource_factory->resource_count = 0;

	resource_factory->header_vtable = NULL;
	return TRUE;
}

/** Register MRCP resource */
MRCP_DECLARE(apt_bool_t) mrcp_resource_register(mrcp_resource_factory_t *resource_factory, mrcp_resource_t *resource)
{
	if(!resource) {
		return FALSE;
	}
	if(resource->id >= resource_factory->resource_count) {
		return FALSE;
	}
	if(resource_factory->resource_array[resource->id]) {
		return FALSE;
	}
	resource->name = mrcp_resource_name_get(resource_factory,resource->id);
	if(mrcp_resource_validate(resource) != TRUE) {
		return FALSE;
	}
	resource_factory->resource_array[resource->id] = resource;
	return TRUE;
}
