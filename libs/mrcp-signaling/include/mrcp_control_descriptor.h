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

#ifndef __MRCP_CONTROL_DESCRIPTOR_H__
#define __MRCP_CONTROL_DESCRIPTOR_H__

/**
 * @file mrcp_control_descriptor.h
 * @brief MRCP Control Descriptor
 */ 

#include "mrcp_types.h"
#include "mpf_media_descriptor.h"
#include "mpf_stream_mode.h"

APT_BEGIN_EXTERN_C

/** MRCP control descriptor declaration */
typedef struct mrcp_control_descriptor_t mrcp_control_descriptor_t;

/** MRCP control descriptor */
struct mrcp_control_descriptor_t {
	mpf_media_descriptor_t base;

	apt_str_t              resource_name;
	apr_size_t             cmid;

	/** MRCP version dependent connection descriptor */
	void                  *connection_descriptor;
};

/** Initialize MRCP control descriptor */
static APR_INLINE void mrcp_control_descriptor_init(mrcp_control_descriptor_t *descriptor)
{
	mpf_media_descriptor_init(&descriptor->base);
	apt_string_reset(&descriptor->resource_name);
	descriptor->cmid = 0;
	descriptor->connection_descriptor = NULL;
}

APT_END_EXTERN_C

#endif /*__MRCP_CONTROL_DESCRIPTOR_H__*/
