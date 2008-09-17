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

#include "rtsp_header.h"
#include "apt_string_table.h"

/** String table of RTSP header fields (rtsp_header_field_id) */
static const apt_str_table_item_t rtsp_header_string_table[] = {
	{{"CSeq",           4},1},
	{{"Transport",      9},0},
	{{"Session",        7},0},
	{{"RTP-Info",       8},0},
	{{"Content-Type",  12},8},
	{{"Content-Length",14},8}
};

/** String table of RTSP content types (rtsp_content_type) */
static const apt_str_table_item_t rtsp_content_type_string_table[] = {
	{{"application/sdp", 15},12},
	{{"application/mrcp",16},12}
};

/** String table of RTSP trasnport profiles (rtsp_transport_profile_t) */
static const apt_str_table_item_t rtsp_profile_string_table[] = {
	{{"RTP/AVP", 7},4},
	{{"RTP/SAVP",8},4}
};

/** String table of RTSP trasnport port types (rtsp_transport_port_type_e) */
static const apt_str_table_item_t rtsp_transport_port_string_table[] = {
	{{"client_port", 11},0},
	{{"server_port", 11},0}
};

/** String table of RTSP trasnport delivery param (rtsp_delivery_t) */
static const apt_str_table_item_t rtsp_delivery_string_table[] = {
	{{"unicast",  7},0},
	{{"multicast",9},0}
};

/** Parse RTSP transport */
static apt_bool_t rtsp_trasnport_parse(rtsp_transport_t *transport, const apt_str_t *line)
{
	/* to be done */
	transport->profile = RTSP_PROFILE_RTP_AVP;
	transport->lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	transport->delivery = RTSP_DELIVERY_UNICAST;

	return TRUE;
}

/** Generate RTSP trasnport port type */
static apt_bool_t rtsp_trasnport_port_type_generate(rtsp_transport_port_type_e type, rtsp_port_range_t *port_range, apt_text_stream_t *text_stream)
{
	const apt_str_t *str;
	str = apt_string_table_str_get(rtsp_transport_port_string_table,RTSP_TRANSPORT_PORT_TYPE_COUNT,type);
	if(!str) {
		return FALSE;
	}
	apt_string_value_generate(str,text_stream);
	apt_text_char_insert(text_stream,'=');
	apt_size_value_generate(port_range->min,text_stream);
	apt_text_char_insert(text_stream,'-');
	apt_size_value_generate(port_range->max,text_stream);
	return TRUE;
}

/** Generate RTSP trasnport */
static apt_bool_t rtsp_trasnport_generate(rtsp_transport_t *transport, apt_text_stream_t *text_stream)
{
	const apt_str_t *profile = apt_string_table_str_get(rtsp_profile_string_table,RTSP_PROFILE_COUNT,transport->profile);
	if(!profile) {
		return FALSE;
	}
	apt_string_value_generate(profile,text_stream);

	if(transport->delivery != RTSP_DELIVERY_NONE) {
		const apt_str_t *delivery = apt_string_table_str_get(rtsp_delivery_string_table,RTSP_DELIVERY_COUNT,transport->delivery);
		if(!delivery) {
			return FALSE;
		}
	
		apt_text_char_insert(text_stream,';');
		apt_string_value_generate(delivery,text_stream);
	}

	if(transport->client_port_range.min != transport->client_port_range.max) {
		apt_text_char_insert(text_stream,';');
		rtsp_trasnport_port_type_generate(RTSP_TRANSPORT_CLIENT_PORT,&transport->client_port_range,text_stream);
	}
	if(transport->server_port_range.min != transport->server_port_range.max) {
		apt_text_char_insert(text_stream,';');
		rtsp_trasnport_port_type_generate(RTSP_TRANSPORT_SERVER_PORT,&transport->server_port_range,text_stream);
	}
	return TRUE;
}


/** Parse RTSP header field */
static apt_bool_t rtsp_header_field_parse(rtsp_header_t *header, rtsp_header_field_id id, const apt_str_t *value, apr_pool_t *pool)
{
	apt_bool_t status = TRUE;
	switch(id) {
		case RTSP_HEADER_FIELD_CSEQ:
			header->cseq = apt_size_value_parse(value);
			break;
		case RTSP_HEADER_FIELD_TRANSPORT:
			rtsp_trasnport_parse(&header->transport,value);
			break;
		case RTSP_HEADER_FIELD_SESSION_ID:
			apt_string_copy(&header->session_id,value,pool);
			break;
		case RTSP_HEADER_FIELD_RTP_INFO:
			apt_string_copy(&header->rtp_info,value,pool);
			break;
		case RTSP_HEADER_FIELD_CONTENT_TYPE:
			header->content_type = apt_string_table_id_find(rtsp_content_type_string_table,RTSP_CONTENT_TYPE_COUNT,value);
			break;
		case RTSP_HEADER_FIELD_CONTENT_LENGTH:
			header->content_length = apt_size_value_parse(value);
			break;
		default:
			status = FALSE;
	}
	return status;
}

/** Generate RTSP header field */
static apr_size_t rtsp_header_field_generate(rtsp_header_t *header, apr_size_t id, apt_text_stream_t *value)
{
	switch(id) {
		case RTSP_HEADER_FIELD_CSEQ:
			apt_size_value_generate(header->cseq,value);
			break;
		case RTSP_HEADER_FIELD_TRANSPORT:
			rtsp_trasnport_generate(&header->transport,value);
			break;
		case RTSP_HEADER_FIELD_SESSION_ID:
			apt_string_value_generate(&header->session_id,value);
			break;
		case RTSP_HEADER_FIELD_RTP_INFO:
			apt_string_value_generate(&header->rtp_info,value);
			break;
		case RTSP_HEADER_FIELD_CONTENT_TYPE:
		{
			const apt_str_t *name = apt_string_table_str_get(rtsp_content_type_string_table,RTSP_CONTENT_TYPE_COUNT,header->content_type);
			if(name) {
				apt_string_value_generate(name,value);
			}
			break;
		}
		case RTSP_HEADER_FIELD_CONTENT_LENGTH:
			apt_size_value_generate(header->content_length,value);
			break;
		default:
			break;
	}
	return TRUE;
}

/** Parse RTSP header */
RTSP_DECLARE(apt_bool_t) rtsp_header_parse(rtsp_header_t *header, apt_text_stream_t *text_stream, apr_pool_t *pool)
{
	apt_name_value_t pair;

	do {
		if(apt_text_header_read(text_stream,&pair) == TRUE) {
			/* parse header_field (name/value) */
			rtsp_header_field_id id = apt_string_table_id_find(rtsp_header_string_table,RTSP_HEADER_FIELD_COUNT,&pair.name);
			if(id < RTSP_HEADER_FIELD_COUNT) {
				if(rtsp_header_field_parse(header,id,&pair.value,pool) == TRUE) {
					rtsp_header_property_add(&header->property_set,id);
				}
			}
		}
		/* length == 0 && !buf -> empty header, exit */
		/* length == 0 && buf -> malformed header, skip to the next one */
	}
	while(pair.name.length || pair.name.buf);

	return TRUE;
}

/** Generate RTSP header */
RTSP_DECLARE(apt_bool_t) rtsp_header_generate(rtsp_header_t *header, apt_text_stream_t *text_stream)
{
	const apt_str_t *name;
	apr_size_t i;
	rtsp_header_property_t property_set;

	property_set = header->property_set;
	for(i=0; i<RTSP_HEADER_FIELD_COUNT && property_set != 0; i++) {
		if(rtsp_header_property_check(&property_set,i) == TRUE) {
			name = apt_string_table_str_get(rtsp_header_string_table,RTSP_HEADER_FIELD_COUNT,i);
			if(!name) {
				continue;
			}
			
			apt_text_header_name_generate(name,text_stream);
			rtsp_header_field_generate(header,i,text_stream);
			apt_text_eol_insert(text_stream);
			
			rtsp_header_property_remove(&property_set,i);
		}
	}

	apt_text_eol_insert(text_stream);
	return TRUE;
}
