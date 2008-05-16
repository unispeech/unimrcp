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

#ifndef __MRCP_CONTROL_CHANNEL_H__
#define __MRCP_CONTROL_CHANNEL_H__

/**
 * @file mrcp_control_channel.h
 * @brief MRCP Control Channel
 */ 

#include "mrcp_types.h"

APT_BEGIN_EXTERN_C

/** MRCP control channel */
typedef struct mrcp_control_channel_t mrcp_control_channel_t;
/** MRCP control channel vtable */
typedef struct mrcp_control_channel_vtable_t mrcp_control_channel_vtable_t;

/** MRCP message dispatcher */
typedef apt_bool_t (*mrcp_message_dispatcher_f)(mrcp_control_channel_t *channel, mrcp_message_t *message);

/** MRCP control channel */
struct mrcp_control_channel_t {
	/** External object associated with the channel */
	void                     *obj;
	/** MRCP message dispatcher */
	mrcp_message_dispatcher_f dispatcher;

	/** Resource specific data */
	void                     *resource_data;

	/** Table of virtual methods of the control channel */
	const mrcp_control_channel_vtable_t *vtable;
};

/** MRCP control channel vtable */
struct mrcp_control_channel_vtable_t {
	/** Virtual process */
	apt_bool_t (*process)(mrcp_control_channel_t *channel, mrcp_message_t *message);
	/** Virtual destroy */
	void (*destroy)(mrcp_control_channel_t *channel);
};

/** Process MRCP message */
static APR_INLINE apt_bool_t mrcp_control_channel_process(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	return channel->vtable->process(channel,message);
}

/** Destroy MRCP control channel */
static APR_INLINE void mrcp_control_channel_destroy(mrcp_control_channel_t *channel)
{
	if(channel->vtable->destroy) {
		channel->vtable->destroy(channel);
	}
}

APT_END_EXTERN_C

#endif /*__MRCP_CONTROL_CHANNEL_H__*/
