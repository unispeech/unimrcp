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
#include "mrcp_control_descriptor.h"

/** String table of mrcp proto types (mrcp_proto_type_e) */
static const apt_str_table_item_t mrcp_proto_type_table[] = {
	{"TCP/MRCPv2",    10,4},
	{"TCP/TLS/MRCPv2",14,4}
};

/** String table of mrcp attributes (mrcp_attrib_e) */
static const apt_str_table_item_t mrcp_attrib_table[] = {
	{"setup",      5,0},
	{"connection",10,1},
	{"resource",   8,0},
	{"channel",    7,1},
	{"cmid",       4,1}
};

/** String table of mrcp setup attribute values (mrcp_setup_type_e) */
static const apt_str_table_item_t mrcp_setup_value_table[] = {
	{"active",      6,0},
	{"passive",     7,0}
};

/** String table of mrcp connection attribute values (mrcp_connection_type_e) */
static const apt_str_table_item_t mrcp_connection_value_table[] = {
	{"new",         3,0},
	{"existing",    8,0}
};

/** String table of mrcp audio attributes (mrcp_audio_attrib_e) */
static const apt_str_table_item_t mrcp_audio_attrib_table[] = {
	{"rtpmap",   6,1},
	{"sendonly", 8,8},
	{"recvonly", 8,2},
	{"sendrecv", 8,4},
	{"mid",      3,0},
	{"ptime",    5,0}
};


MRCP_DECLARE(const apt_str_t*) mrcp_proto_get(mrcp_proto_type_e proto)
{
	return apt_string_table_str_get(mrcp_proto_type_table,MRCP_PROTO_COUNT,proto);
}

MRCP_DECLARE(mrcp_proto_type_e) mrcp_proto_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_proto_type_table,MRCP_PROTO_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_attrib_str_get(mrcp_attrib_e attrib_id)
{
	return apt_string_table_str_get(mrcp_attrib_table,MRCP_ATTRIB_COUNT,attrib_id);
}

MRCP_DECLARE(mrcp_attrib_e) mrcp_attrib_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_attrib_table,MRCP_ATTRIB_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_setup_type_get(mrcp_setup_type_e setup_type)
{
	return apt_string_table_str_get(mrcp_setup_value_table,MRCP_SETUP_TYPE_COUNT,setup_type);
}

MRCP_DECLARE(mrcp_setup_type_e) mrcp_setup_type_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_setup_value_table,MRCP_SETUP_TYPE_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_connection_type_get(mrcp_connection_type_e connection_type)
{
	return apt_string_table_str_get(mrcp_connection_value_table,MRCP_CONNECTION_TYPE_COUNT,connection_type);
}

MRCP_DECLARE(mrcp_connection_type_e) mrcp_connection_type_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_connection_value_table,MRCP_CONNECTION_TYPE_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_audio_attrib_str_get(mrcp_audio_attrib_e attrib_id)
{
	return apt_string_table_str_get(mrcp_audio_attrib_table,MRCP_AUDIO_ATTRIB_COUNT,attrib_id);
}

MRCP_DECLARE(mrcp_audio_attrib_e) mrcp_audio_attrib_id_find(const apt_str_t *attrib)
{
	return apt_string_table_id_find(mrcp_audio_attrib_table,MRCP_AUDIO_ATTRIB_COUNT,attrib);
}

MRCP_DECLARE(const apt_str_t*) mrcp_audio_direction_str_get(mpf_stream_mode_e direction)
{
	mrcp_audio_attrib_e attrib_id = MRCP_AUDIO_ATTRIB_UNKNOWN;
	switch(direction) {
		case STREAM_MODE_SEND:
			attrib_id = MRCP_AUDIO_ATTRIB_SENDONLY;
			break;
		case STREAM_MODE_RECEIVE:
			attrib_id = MRCP_AUDIO_ATTRIB_RECVONLY;
			break;
		case STREAM_MODE_SEND_RECEIVE:
			attrib_id = MRCP_AUDIO_ATTRIB_SENDRECV;
			break;
		default:
			break;
	}
	return apt_string_table_str_get(mrcp_audio_attrib_table,MRCP_AUDIO_ATTRIB_COUNT,attrib_id);
}
