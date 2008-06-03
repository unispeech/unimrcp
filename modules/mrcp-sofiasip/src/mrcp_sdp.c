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
#include "mrcp_sdp.h"
#include "mrcp_session_descriptor.h"
#include "mrcpv2_connection_descriptor.h"
#include "mpf_rtp_attribs.h"
#include "apt_text_stream.h"
#include "apt_log.h"

static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_descriptor);

static apt_bool_t mpf_rtp_media_generate(mpf_rtp_media_descriptor_t *audio_media, const sdp_media_t *sdp_media, apr_pool_t *pool);
static apt_bool_t mrcp_control_media_generate(mrcp_control_descriptor_t *mrcp_media, const sdp_media_t *sdp_media, apr_pool_t *pool);
static apt_bool_t mpf_media_generate(mpf_media_descriptor_t *media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool);

/** Generate SDP string by MRCP descriptor */
MRCP_DECLARE(apr_size_t) sdp_string_generate_by_mrcp_descriptor(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, apt_bool_t offer)
{
	apr_size_t i;
	apr_size_t count;
	apr_size_t audio_index = 0;
	mpf_rtp_media_descriptor_t *audio_media;
	apr_size_t video_index = 0;
	mpf_rtp_media_descriptor_t *video_media;
	apr_size_t control_index = 0;
	mrcp_control_descriptor_t *control_media;
	mrcp_connection_descriptor_t *connection_descriptor;
	apr_size_t offset = 0;
	buffer[0] = '\0';
	offset += snprintf(buffer+offset,size-offset,
			"v=0\r\n"
			"o=%s 0 0 IN IP4 %s\r\n"
			"s=-\r\n"
			"c=IN IP4 %s\r\n"
			"t=0 0\r\n",
			descriptor->origin.buf ? descriptor->origin.buf : "-",
			descriptor->ip.buf,
			descriptor->ip.buf);
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
		control_media = mrcp_session_control_media_get(descriptor,control_index);
		if(control_media && control_media->base.id == i) {
			/** generate mrcp control media */
			const apt_str_t *proto;
			const apt_str_t *setup_type;
			const apt_str_t *connection_type;
			connection_descriptor = control_media->connection_descriptor;
			proto = mrcp_proto_get(connection_descriptor->proto);
			setup_type = mrcp_setup_type_get(connection_descriptor->setup_type);
			connection_type = mrcp_connection_type_get(connection_descriptor->connection_type);
			control_index++;
			if(offer == TRUE) { /* offer */
				offset += snprintf(buffer+offset,size-offset,
					"m=application %d %s 1\r\n"
					"a=setup:%s\r\n"
					"a=connection:%s\r\n"
					"a=resource:%s\r\n"
					"a=cmid:%d\r\n",
					(control_media->base.state == MPF_MEDIA_ENABLED) ? control_media->base.port : 0,
					proto ? proto->buf : "",
					setup_type ? setup_type->buf : "",
					connection_type ? connection_type->buf : "",
					control_media->resource_name,
					control_media->cmid);
			}
			else { /* answer */
				offset += sprintf(buffer+offset,
					"m=application %d %s 1\r\n"
					"a=setup:%s\r\n"
					"a=connection:%s\r\n"
					"a=channel:%s@%s\r\n"
					"a=cmid:%d\r\n",
					(control_media->base.state == MPF_MEDIA_ENABLED) ? control_media->base.port : 0,
					proto ? proto->buf : "",
					setup_type ? setup_type->buf : "",
					connection_type ? connection_type->buf : "",
					connection_descriptor->session_id.buf,
					control_media->resource_name.buf,
					control_media->cmid);
			}
			continue;
		}
	}
	return offset;
}

/** Generate MRCP descriptor by SDP session */
MRCP_DECLARE(mrcp_session_descriptor_t*) mrcp_descriptor_generate_by_sdp_session(const sdp_session_t *sdp, apr_pool_t *pool)
{
	sdp_media_t *sdp_media;
	mpf_media_descriptor_t *base;
	mrcp_session_descriptor_t *descriptor = mrcp_session_descriptor_create(pool);

	if(sdp->sdp_connection) {
		apt_string_assign(&descriptor->ip,sdp->sdp_connection->c_address,pool);
	}

	for(sdp_media=sdp->sdp_media; sdp_media; sdp_media=sdp_media->m_next) {
		base = NULL;
		switch(sdp_media->m_type) {
			case sdp_media_audio:
			{
				mpf_rtp_media_descriptor_t *media = mrcp_session_audio_media_add(descriptor);
				base = &media->base;
				mpf_rtp_media_generate(media,sdp_media,pool);
				break;
			}
			case sdp_media_video:
			{
				mpf_rtp_media_descriptor_t *media = mrcp_session_video_media_add(descriptor);
				base = &media->base;
				mpf_rtp_media_generate(media,sdp_media,pool);
				break;
			}
			case sdp_media_application:
			{
				mrcp_control_descriptor_t *control_media = mrcp_session_control_media_add(descriptor);
				base = &control_media->base;
				mrcp_control_media_generate(control_media,sdp_media,pool);
				break;
			}
			default:
				apt_log(APT_PRIO_INFO,"Not Supported SDP Media [%s]\n", sdp_media->m_type_name);
				break;
		}
		if(base) {
			mpf_media_generate(base,sdp_media,&descriptor->ip,pool);
		}
	}
	return descriptor;
}


/** Generate SDP media by RTP media descriptor */
static apr_size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_media)
{
	apr_size_t offset = 0;
	int i;
	mpf_codec_descriptor_t *codec_descriptor;
	apr_array_header_t *descriptor_arr = audio_media->codec_list.descriptor_arr;
	offset += snprintf(buffer+offset,size-offset,
		"m=audio %d RTP/AVP", 
		audio_media->base.state == MPF_MEDIA_ENABLED ? audio_media->base.port : 0);
	for(i=0; i<descriptor_arr->nelts; i++) {
		codec_descriptor = (mpf_codec_descriptor_t*)descriptor_arr->elts + i;
		offset += snprintf(buffer+offset,size-offset," %d", codec_descriptor->payload_type);
	}
	offset += snprintf(buffer+offset,size-offset,"\r\n");
	if(descriptor->ip.length && audio_media->base.ip.length && 
		apt_string_compare(&descriptor->ip,&audio_media->base.ip) != TRUE) {
		offset += sprintf(buffer+offset,"c=IN IP4 %s\r\n",audio_media->base.ip.buf);
	}
	if(audio_media->base.state == MPF_MEDIA_ENABLED) {
		for(i=0; i<descriptor_arr->nelts; i++) {
			codec_descriptor = (mpf_codec_descriptor_t*)descriptor_arr->elts + i;
			offset += snprintf(buffer+offset,size-offset,"a=rtpmap:%d %s/%d\r\n",
				codec_descriptor->payload_type,
				codec_descriptor->name.buf,
				codec_descriptor->sampling_rate);
		}
		offset += snprintf(buffer+offset,size-offset,"a=%s\r\n",
			mpf_stream_mode_str_get(audio_media->mode));
		
		if(audio_media->ptime) {
			offset += snprintf(buffer+offset,size-offset,"a=ptime:%hu\r\n",
				audio_media->ptime);
		}
	}
	offset += snprintf(buffer+offset,size-offset,"a=mid:%d\r\n",audio_media->mid);
	return offset;
}

/** Generate RTP media descriptor by SDP media */
static apt_bool_t mpf_rtp_media_generate(mpf_rtp_media_descriptor_t *audio_media, const sdp_media_t *sdp_media, apr_pool_t *pool)
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
			case RTP_ATTRIB_MID:
				audio_media->mid = atoi(attrib->a_value);
				break;
			case RTP_ATTRIB_PTIME:
				audio_media->ptime = (unsigned short)atoi(attrib->a_value);
				break;
			default:
				break;
		}
	}

	mpf_codec_list_init(&audio_media->codec_list,5,pool);
	for(map = sdp_media->m_rtpmaps; map; map = map->rm_next) {
		codec = mpf_codec_list_add(&audio_media->codec_list);
		if(codec) {
			codec->payload_type = (apr_byte_t)map->rm_pt;
			apt_string_assign(&codec->name,map->rm_encoding,pool);
			codec->sampling_rate = (apr_uint16_t)map->rm_rate;
			codec->channel_count = 1;
		}
	}

	switch(sdp_media->m_mode) {
		case sdp_inactive:
			audio_media->mode = STREAM_MODE_NONE;
			break;
		case sdp_sendonly:
			audio_media->mode = STREAM_MODE_SEND;
			break;
		case sdp_recvonly:
			audio_media->mode = STREAM_MODE_RECEIVE;
			break;
		case sdp_sendrecv:
			audio_media->mode = STREAM_MODE_SEND_RECEIVE;
			break;
	}
	return TRUE;
}

/** Generate MRCP control media by SDP media */
static apt_bool_t mrcp_control_media_generate(mrcp_control_descriptor_t *mrcp_media, const sdp_media_t *sdp_media, apr_pool_t *pool)
{
	mrcp_connection_descriptor_t *connection_descriptor;
	mrcp_attrib_e id;
	apt_str_t name;
	apt_str_t value;
	sdp_attribute_t *attrib = NULL;
	apt_string_set(&name,sdp_media->m_proto_name);
	connection_descriptor = apr_palloc(pool,sizeof(mrcp_connection_descriptor_t));
	mrcp_media->connection_descriptor = connection_descriptor;
	mrcp_connection_descriptor_init(connection_descriptor);
	connection_descriptor->proto = mrcp_proto_find(&name);
	if(connection_descriptor->proto != MRCP_PROTO_TCP) {
		apt_log(APT_PRIO_INFO,"Not supported SDP Proto [%s], expected [%s]\n",sdp_media->m_proto_name,mrcp_proto_get(MRCP_PROTO_TCP));
		return FALSE;
	}
	
	for(attrib = sdp_media->m_attributes; attrib; attrib=attrib->a_next) {
		apt_string_set(&name,attrib->a_name);
		id = mrcp_attrib_id_find(&name);
		switch(id) {
			case MRCP_ATTRIB_SETUP:
				apt_string_set(&value,attrib->a_value);
				connection_descriptor->setup_type = mrcp_setup_type_find(&value);
				break;
			case MRCP_ATTRIB_CONNECTION:
				apt_string_set(&value,attrib->a_value);
				connection_descriptor->connection_type = mrcp_connection_type_find(&value);
				break;
			case MRCP_ATTRIB_RESOURCE:
				apt_string_assign(&mrcp_media->resource_name,attrib->a_value,pool);
				break;
			case MRCP_ATTRIB_CHANNEL:
				apt_string_set(&value,attrib->a_value);
				apt_id_resource_parse(&value,'@',&connection_descriptor->session_id,&connection_descriptor->resource_name,pool);
				break;
			case MRCP_ATTRIB_CMID:
				mrcp_media->cmid = atoi(attrib->a_value);
				break;
			default:
				break;
		}
	}
	return TRUE;
}

/** Generate media descriptor base by SDP media */
static apt_bool_t mpf_media_generate(mpf_media_descriptor_t *media, const sdp_media_t *sdp_media, const apt_str_t *ip, apr_pool_t *pool)
{
	if(sdp_media->m_connections) {
		apt_string_assign(&media->ip,sdp_media->m_connections->c_address,pool);
	}
	else {
		media->ip = *ip;
	}
	if(sdp_media->m_port) {
		media->port = (unsigned short)sdp_media->m_port;
		media->state = MPF_MEDIA_ENABLED;
	}
	else {
		media->state = MPF_MEDIA_DISABLED;
	}
	return TRUE;
}
