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

#include "apt_string_table.h"
#include "mrcp_generic_header.h"

/** String table of mrcp generic-header fields (mrcp_generic_header_id) */
static const apt_str_table_item_t generic_header_string_table[] = {
	{{"Active-Request-Id-List",22},2},
	{{"Proxy-Sync-Id",         13},0},
	{{"Accept-Charset",        14},3},
	{{"Content-Type",          12},9},
	{{"Content-Id",            10},8},
	{{"Content-Base",          12},8},
	{{"Content-Encoding",      16},8},
	{{"Content-Location",      16},9},
	{{"Content-Length",        14},9},
	{{"Cache-Control",         13},1},
	{{"Logging-Tag",           11},0}, 
};


/** Parse mrcp request-id list */
static apt_bool_t mrcp_request_id_list_parse(mrcp_request_id_list_t *request_id_list, apt_text_stream_t *stream)
{
	apt_str_t field;
	request_id_list->count = 0;
	while(stream->pos < stream->text.buf + stream->text.length && request_id_list->count < MAX_ACTIVE_REQUEST_ID_COUNT) {
		apt_text_field_read(stream,',',TRUE,&field);
		if(field.length) {
			request_id_list->ids[request_id_list->count] = apt_size_value_parse(field.buf);
			request_id_list->count++;
		}
	}
	return TRUE;
}

/** Generate mrcp request-id list */
static apt_bool_t mrcp_request_id_list_generate(mrcp_request_id_list_t *request_id_list, apt_text_stream_t *stream)
{
	size_t i;
	for(i=0; i<request_id_list->count; i++) {
		apt_size_value_generate(request_id_list->ids[i],stream);
		if(i < request_id_list->count-1) {
			*stream->pos++ = ',';
		}
	}
	return TRUE;
}


/** Initialize generic-header */
static void mrcp_generic_header_init(mrcp_generic_header_t *generic_header)
{
	generic_header->active_request_id_list.count = 0;
	apt_string_reset(&generic_header->proxy_sync_id);
	apt_string_reset(&generic_header->accept_charset);
	apt_string_reset(&generic_header->content_type);
	apt_string_reset(&generic_header->content_id);
	apt_string_reset(&generic_header->content_base);
	apt_string_reset(&generic_header->content_encoding);
	apt_string_reset(&generic_header->content_location);
	generic_header->content_length = 0;
	apt_string_reset(&generic_header->cache_control);
	apt_string_reset(&generic_header->logging_tag);
}


/** Allocate generic-header */
static void* mrcp_generic_header_allocate(mrcp_header_accessor_t *accessor, apr_pool_t *pool)
{
	mrcp_generic_header_t *generic_header = apr_palloc(pool,sizeof(mrcp_generic_header_t));
	mrcp_generic_header_init(generic_header);
	accessor->data = generic_header;
	return accessor->data;
}

/** Destroy generic-header */
static void mrcp_generic_header_destroy(mrcp_header_accessor_t *accessor)
{
	if(accessor->data) {
		mrcp_generic_header_destroy(accessor->data);
		accessor->data = NULL;
	}
}

/** Parse generic-header */
static apt_bool_t mrcp_generic_header_parse(mrcp_header_accessor_t *accessor, size_t id, apt_text_stream_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	mrcp_generic_header_t *generic_header = accessor->data;
	apr_size_t length = value->text.length - (value->pos - value->text.buf);
	switch(id)
	{
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_parse(&generic_header->active_request_id_list,value);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			apt_string_assign_n(&generic_header->proxy_sync_id,value->pos,length,pool);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_assign_n(&generic_header->accept_charset,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_assign_n(&generic_header->content_type,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_assign_n(&generic_header->content_id,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_assign_n(&generic_header->content_base,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_assign_n(&generic_header->content_encoding,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_assign_n(&generic_header->content_location,value->pos,length,pool);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = apt_size_value_parse(value->pos);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_assign_n(&generic_header->cache_control,value->pos,length,pool);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_assign_n(&generic_header->logging_tag,value->pos,length,pool);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate generic-header */
static apt_bool_t mrcp_generic_header_generate(mrcp_header_accessor_t *accessor, size_t id, apt_text_stream_t *value)
{
	mrcp_generic_header_t *generic_header = accessor->data;
	switch(id) {
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			mrcp_request_id_list_generate(&generic_header->active_request_id_list,value);
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			apt_string_value_generate(&generic_header->proxy_sync_id,value);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_value_generate(&generic_header->accept_charset,value);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_value_generate(&generic_header->content_type,value);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_value_generate(&generic_header->content_id,value);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_value_generate(&generic_header->content_base,value);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_value_generate(&generic_header->content_encoding,value);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_value_generate(&generic_header->content_location,value);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			apt_size_value_generate(generic_header->content_length,value);
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_value_generate(&generic_header->cache_control,value);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_value_generate(&generic_header->logging_tag,value);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Duplicate generic-header */
static apt_bool_t mrcp_generic_header_duplicate(mrcp_header_accessor_t *accessor, const mrcp_header_accessor_t *src, size_t id, apr_pool_t *pool)
{
	mrcp_generic_header_t *generic_header = accessor->data;
	const mrcp_generic_header_t *src_generic_header = src->data;
	apt_bool_t status = TRUE;

	if(!generic_header || !src_generic_header) {
		return FALSE;
	}

	switch(id)
	{
		case GENERIC_HEADER_ACTIVE_REQUEST_ID_LIST:
			break;
		case GENERIC_HEADER_PROXY_SYNC_ID:
			apt_string_copy(&generic_header->proxy_sync_id,&src_generic_header->proxy_sync_id,pool);
			break;
		case GENERIC_HEADER_ACCEPT_CHARSET:
			apt_string_copy(&generic_header->accept_charset,&src_generic_header->accept_charset,pool);
			break;
		case GENERIC_HEADER_CONTENT_TYPE:
			apt_string_copy(&generic_header->content_type,&src_generic_header->content_type,pool);
			break;
		case GENERIC_HEADER_CONTENT_ID:
			apt_string_copy(&generic_header->content_id,&src_generic_header->content_id,pool);
			break;
		case GENERIC_HEADER_CONTENT_BASE:
			apt_string_copy(&generic_header->content_base,&src_generic_header->content_base,pool);
			break;
		case GENERIC_HEADER_CONTENT_ENCODING:
			apt_string_copy(&generic_header->content_encoding,&src_generic_header->content_encoding,pool);
			break;
		case GENERIC_HEADER_CONTENT_LOCATION:
			apt_string_copy(&generic_header->content_location,&src_generic_header->content_location,pool);
			break;
		case GENERIC_HEADER_CONTENT_LENGTH:
			generic_header->content_length = src_generic_header->content_length;
			break;
		case GENERIC_HEADER_CACHE_CONTROL:
			apt_string_copy(&generic_header->cache_control,&src_generic_header->cache_control,pool);
			break;
		case GENERIC_HEADER_LOGGING_TAG:
			apt_string_copy(&generic_header->logging_tag,&src_generic_header->logging_tag,pool);
			break;
		default:
			status = FALSE;
	}
	return status;
}

static const mrcp_header_vtable_t vtable = {
	mrcp_generic_header_allocate,
	mrcp_generic_header_destroy,
	mrcp_generic_header_parse,
	mrcp_generic_header_generate,
	mrcp_generic_header_duplicate,
	generic_header_string_table,
	GENERIC_HEADER_COUNT
};


MRCP_DECLARE(const mrcp_header_vtable_t*) mrcp_generic_header_vtable_get()
{
	return &vtable;
}
