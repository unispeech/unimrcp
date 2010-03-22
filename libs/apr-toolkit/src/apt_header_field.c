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

#include "apt_header_field.h"

/** Initialize header section (collection of header fields) */
APT_DECLARE(void) apt_header_section_init(apt_header_section_t *header, int max_field_count, apr_pool_t *pool)
{
	APR_RING_INIT(&header->ring, apt_header_field_t, link);
	header->arr = (apt_header_field_t**)apr_pcalloc(pool,sizeof(apt_header_field_t*) * max_field_count);
	header->arr_size = max_field_count;
}

/** Add header field to header section */
APT_DECLARE(apt_bool_t) apt_header_section_field_add(apt_header_section_t *header, apt_header_field_t *header_field, int id)
{
	if(id >= header->arr_size) {
		return FALSE;
	}
	if(header->arr[id]) {
		return FALSE;
	}
	header->arr[id] = header_field;
	APR_RING_INSERT_TAIL(&header->ring,header_field,apt_header_field_t,link);
	return TRUE;
}

/** Remove header field from header section */
APT_DECLARE(apt_bool_t) apt_header_section_field_remove(apt_header_section_t *header, int id)
{
	apt_header_field_t *header_field;
	if(id >= header->arr_size) {
		return FALSE;
	}
	if(!header->arr[id]) {
		return FALSE;
	}
	header_field = header->arr[id];
	header->arr[id] = NULL;
	APR_RING_REMOVE(header_field,link);
	return TRUE;
}
