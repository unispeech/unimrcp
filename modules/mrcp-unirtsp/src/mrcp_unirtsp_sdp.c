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

#include <apr_general.h>
#include <sofia-sip/sdp.h>
#include "rtsp_message.h"
#include "mrcp_unirtsp_sdp.h"
#include "mpf_rtp_attribs.h"
#include "apt_text_stream.h"
#include "apt_log.h"


/** Generate SDP media by RTP media descriptor */
static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_media)
{
	apr_size_t offset = 0;
	int i;
	mpf_codec_descriptor_t *codec_descriptor;
	apr_array_header_t *descriptor_arr = audio_media->codec_list.descriptor_arr;
	if(!descriptor_arr) {
		return 0;
	}
	offset += snprintf(buffer+offset,size-offset,
		"m=audio %d RTP/AVP", 
		audio_media->base.state == MPF_MEDIA_ENABLED ? audio_media->base.port : 0);
	for(i=0; i<descriptor_arr->nelts; i++) {
		codec_descriptor = (mpf_codec_descriptor_t*)descriptor_arr->elts + i;
		offset += snprintf(buffer+offset,size-offset," %d", codec_descriptor->payload_type);
	}
	offset += snprintf(buffer+offset,size-offset,"\r\n");
	if(audio_media->base.state == MPF_MEDIA_ENABLED) {
		const apt_str_t *mode_str = mpf_stream_mode_str_get(audio_media->mode);
		for(i=0; i<descriptor_arr->nelts; i++) {
			codec_descriptor = (mpf_codec_descriptor_t*)descriptor_arr->elts + i;
			if(codec_descriptor->name.buf) {
				offset += snprintf(buffer+offset,size-offset,"a=rtpmap:%d %s/%d\r\n",
					codec_descriptor->payload_type,
					codec_descriptor->name.buf,
					codec_descriptor->sampling_rate);
			}
		}
		offset += snprintf(buffer+offset,size-offset,"a=%s\r\n",mode_str ? mode_str->buf : "");
		
		if(audio_media->ptime) {
			offset += snprintf(buffer+offset,size-offset,"a=ptime:%hu\r\n",
				audio_media->ptime);
		}
	}
	return offset;
}

/** Generate RTP media descriptor by SDP media */
static apt_bool_t mpf_rtp_media_generate(mpf_rtp_media_descriptor_t *rtp_media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool)
{
	mpf_rtp_attrib_e id;
	apt_str_t name;
	sdp_attribute_t *attrib = NULL;
	sdp_rtpmap_t *map;
	mpf_codec_descriptor_t *codec;
	for(attrib = sdp_media->m_attributes; attrib; attrib=attrib->a_next) {
		apt_string_set(&name,attrib->a_name);
		id = mpf_rtp_attrib_id_find(&name);
		switch(id) {
			case RTP_ATTRIB_PTIME:
				rtp_media->ptime = (apr_uint16_t)atoi(attrib->a_value);
				break;
			default:
				break;
		}
	}

	mpf_codec_list_init(&rtp_media->codec_list,5,pool);
	for(map = sdp_media->m_rtpmaps; map; map = map->rm_next) {
		codec = mpf_codec_list_add(&rtp_media->codec_list);
		if(codec) {
			codec->payload_type = (apr_byte_t)map->rm_pt;
			apt_string_assign(&codec->name,map->rm_encoding,pool);
			codec->sampling_rate = (apr_uint16_t)map->rm_rate;
			codec->channel_count = 1;
		}
	}

	switch(sdp_media->m_mode) {
		case sdp_inactive:
			rtp_media->mode = STREAM_MODE_NONE;
			break;
		case sdp_sendonly:
			rtp_media->mode = STREAM_MODE_SEND;
			break;
		case sdp_recvonly:
			rtp_media->mode = STREAM_MODE_RECEIVE;
			break;
		case sdp_sendrecv:
			rtp_media->mode = STREAM_MODE_SEND_RECEIVE;
			break;
	}

	if(sdp_media->m_connections) {
		apt_string_assign(&rtp_media->base.ip,sdp_media->m_connections->c_address,pool);
	}
	else {
		rtp_media->base.ip = *ip;
	}
	if(sdp_media->m_port) {
		rtp_media->base.port = (apr_port_t)sdp_media->m_port;
		rtp_media->base.state = MPF_MEDIA_ENABLED;
	}
	else {
		rtp_media->base.state = MPF_MEDIA_DISABLED;
	}
	return TRUE;
}

/** Generate MRCP descriptor by SDP session */
static mrcp_session_descriptor_t* mrcp_descriptor_generate_by_sdp_session(const sdp_session_t *sdp, apr_pool_t *pool)
{
	sdp_media_t *sdp_media;
	mrcp_session_descriptor_t *descriptor = mrcp_session_descriptor_create(pool);

	if(sdp->sdp_connection) {
		apt_string_assign(&descriptor->ip,sdp->sdp_connection->c_address,pool);
	}

	for(sdp_media=sdp->sdp_media; sdp_media; sdp_media=sdp_media->m_next) {
		switch(sdp_media->m_type) {
			case sdp_media_audio:
			{
				mpf_rtp_media_descriptor_t *media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
				mpf_rtp_media_descriptor_init(media);
				media->base.id = mrcp_session_audio_media_add(descriptor,media);
				mpf_rtp_media_generate(media,sdp_media,&descriptor->ip,pool);
				break;
			}
			case sdp_media_video:
			{
				mpf_rtp_media_descriptor_t *media = apr_palloc(pool,sizeof(mpf_rtp_media_descriptor_t));
				mpf_rtp_media_descriptor_init(media);
				media->base.id = mrcp_session_video_media_add(descriptor,media);
				mpf_rtp_media_generate(media,sdp_media,&descriptor->ip,pool);
				break;
			}
			default:
				apt_log(APT_PRIO_INFO,"Not Supported SDP Media [%s]", sdp_media->m_type_name);
				break;
		}
	}
	return descriptor;
}


/** Generate MRCP descriptor by RTSP request */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_request(const rtsp_message_t *request, apr_pool_t *pool, su_home_t *home)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	const char *resource_name = request->start_line.common.request_line.resource_name;
	if(!resource_name) {
		return NULL;
	}
	
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
		if(rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
			rtsp_header_property_check(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
			request->body.buf) {
			
			sdp_parser_t *parser;
			sdp_session_t *sdp;

			parser = sdp_parse(home,request->body.buf,request->body.length,0);
			sdp = sdp_session(parser);

			descriptor = mrcp_descriptor_generate_by_sdp_session(sdp,pool);
			if(descriptor) {
				apt_string_assign(&descriptor->resource_name,resource_name,pool);
				descriptor->resource_state = TRUE;
			}
			
			sdp_parser_free(parser);
		}
	}
	else if(request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		descriptor = mrcp_session_descriptor_create(pool);
		if(descriptor) {
			apt_string_assign(&descriptor->resource_name,resource_name,pool);
			descriptor->resource_state = FALSE;
		}
	}
	return descriptor;
}

/** Generate MRCP descriptor by RTSP response */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_rtsp_response(const rtsp_message_t *request, const rtsp_message_t *response, apr_pool_t *pool, su_home_t *home)
{
	mrcp_session_descriptor_t *descriptor = NULL;
	const char *resource_name = request->start_line.common.request_line.resource_name;
	if(!resource_name) {
		return NULL;
	}
	
	if(request->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
		if(rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE) == TRUE &&
			rtsp_header_property_check(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH) == TRUE &&
			response->body.buf) {
			
			sdp_parser_t *parser;
			sdp_session_t *sdp;

			parser = sdp_parse(home,response->body.buf,response->body.length,0);
			sdp = sdp_session(parser);

			descriptor = mrcp_descriptor_generate_by_sdp_session(sdp,pool);
			if(descriptor) {
				apt_string_assign(&descriptor->resource_name,resource_name,pool);
				descriptor->resource_state = TRUE;
			}
			
			sdp_parser_free(parser);
		}
	}
	else if(request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
		descriptor = mrcp_session_descriptor_create(pool);
		if(descriptor) {
			apt_string_assign(&descriptor->resource_name,resource_name,pool);
			descriptor->resource_state = FALSE;
		}
	}
	return descriptor;
}

/** Generate RTSP request by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_request_generate_by_mrcp_descriptor(const mrcp_session_descriptor_t *descriptor, apr_pool_t *pool)
{
	apr_size_t i;
	apr_size_t count;
	apr_size_t audio_index = 0;
	mpf_rtp_media_descriptor_t *audio_media;
	apr_size_t video_index = 0;
	mpf_rtp_media_descriptor_t *video_media;
	apr_size_t offset = 0;
	char buffer[2048];
	apr_size_t size = sizeof(buffer);
	rtsp_message_t *request;

	request = rtsp_request_create(pool);
	request->start_line.common.request_line.resource_name = descriptor->resource_name.buf;
	if(descriptor->resource_state != TRUE) {
		request->start_line.common.request_line.method_id = RTSP_METHOD_TEARDOWN;
		return request;
	}

	request->start_line.common.request_line.method_id = RTSP_METHOD_SETUP;

	buffer[0] = '\0';
	offset += snprintf(buffer+offset,size-offset,
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=-\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n",
			descriptor->origin.buf ? descriptor->origin.buf : "-",
			descriptor->ip.buf ? descriptor->ip.buf : "0",
			descriptor->ip.buf ? descriptor->ip.buf : "0");
	count = mrcp_session_media_count_get(descriptor);
	for(i=0; i<count; i++) {
		audio_media = mrcp_session_audio_media_get(descriptor,audio_index);
		if(audio_media && audio_media->base.id == i) {
			/* generate audio media */
			audio_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,audio_media);
			continue;
		}
		video_media = mrcp_session_video_media_get(descriptor,video_index);
		if(video_media && video_media->base.id == i) {
			/* generate video media */
			video_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,video_media);
			continue;
		}
	}

	request->header.transport.profile = RTSP_PROFILE_RTP_AVP;
	request->header.transport.delivery = RTSP_DELIVERY_UNICAST;
	rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_TRANSPORT);

	if(offset) {
		apt_string_assign_n(&request->body,buffer,offset,pool);
		request->header.content_type = RTSP_CONTENT_TYPE_SDP;
		rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
		request->header.content_length = offset;
		rtsp_header_property_add(&request->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);
	}
	return request;
}

/** Generate RTSP response by MRCP descriptor */
MRCP_DECLARE(rtsp_message_t*) rtsp_response_generate_by_mrcp_descriptor(const rtsp_message_t *request, const mrcp_session_descriptor_t *descriptor, apr_pool_t *pool)
{
	apr_size_t i;
	apr_size_t count;
	apr_size_t audio_index = 0;
	mpf_rtp_media_descriptor_t *audio_media;
	apr_size_t video_index = 0;
	mpf_rtp_media_descriptor_t *video_media;
	apr_size_t offset = 0;
	char buffer[2048];
	apr_size_t size = sizeof(buffer);
	rtsp_message_t *response;

	if(descriptor->resource_state != TRUE) {
		response = rtsp_response_create(request,RTSP_STATUS_CODE_NOT_FOUND,RTSP_REASON_PHRASE_NOT_FOUND,pool);
		return response;
	}

	response = rtsp_response_create(request,RTSP_STATUS_CODE_OK,RTSP_REASON_PHRASE_OK,pool);
	if(!response) {
		return NULL;
	}

	buffer[0] = '\0';
	offset += snprintf(buffer+offset,size-offset,
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=-\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n",
			descriptor->origin.buf ? descriptor->origin.buf : "-",
			descriptor->ip.buf ? descriptor->ip.buf : "0",
			descriptor->ip.buf ? descriptor->ip.buf : "0");
	count = mrcp_session_media_count_get(descriptor);
	for(i=0; i<count; i++) {
		audio_media = mrcp_session_audio_media_get(descriptor,audio_index);
		if(audio_media && audio_media->base.id == i) {
			/* generate audio media */
			audio_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,audio_media);
			continue;
		}
		video_media = mrcp_session_video_media_get(descriptor,video_index);
		if(video_media && video_media->base.id == i) {
			/* generate video media */
			video_index++;
			offset += sdp_rtp_media_generate(buffer+offset,size-offset,descriptor,video_media);
			continue;
		}
	}

	/* ok */
	response->header.transport.profile = RTSP_PROFILE_RTP_AVP;
	response->header.transport.delivery = RTSP_DELIVERY_UNICAST;
	rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_TRANSPORT);

	if(offset) {
		apt_string_assign_n(&response->body,buffer,offset,pool);
		response->header.content_type = RTSP_CONTENT_TYPE_SDP;
		rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_TYPE);
		response->header.content_length = offset;
		rtsp_header_property_add(&response->header.property_set,RTSP_HEADER_FIELD_CONTENT_LENGTH);
	}
	return response;
}
