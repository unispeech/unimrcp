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
#include "mrcp_session_descriptor.h"
#include "mpf_rtp_attribs.h"
#include "rtsp_message.h"
#include "apt_text_stream.h"
#include "apt_log.h"


/** Generate SDP media by RTP media descriptor */
static size_t sdp_rtp_media_generate(char *buffer, apr_size_t size, const mrcp_session_descriptor_t *descriptor, const mpf_rtp_media_descriptor_t *audio_media)
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

	return TRUE;
}
