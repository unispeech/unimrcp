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

#ifndef __MRCP_PARSER_H__
#define __MRCP_PARSER_H__

/**
 * @file mrcp_parser.h
 * @brief MRCP Parser
 */ 

#include "mrcp_message.h"
#include "mrcp_resource.h"

APT_BEGIN_EXTERN_C

/** Parse MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_parse(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *text_stream);

/** Generate MRCP message */
MRCP_DECLARE(apt_bool_t) mrcp_message_generate(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message, apt_text_stream_t *text_stream);


/** Associate MRCP resource specific data by resource identifier (mrcp_message->resource_id must be set prior to call of the function) */
MRCP_DECLARE(apt_bool_t) mrcp_message_associate_resource_by_id(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message);

/** Associate MRCP resource specific data by resource name (mrcp_message->resource_name must be set prior to call of the function) */
MRCP_DECLARE(apt_bool_t) mrcp_message_associate_resource_by_name(mrcp_resource_factory_t *resource_factory, mrcp_message_t *message);

APT_END_EXTERN_C

#endif /*__MRCP_PARSER_H__*/
