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

#ifndef __APT_TASK_MSG_H__
#define __APT_TASK_MSG_H__

/**
 * @file apt_task_msg.h
 * @brief Task Message Base Definition
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Enumeration of base task messages */
typedef enum {
	TASK_MSG_NONE,
	TASK_MSG_START_COMPLETE,
	TASK_MSG_TERMINATE_REQUEST,
	TASK_MSG_TERMINATE_COMPLETE,

	TASK_MSG_USER,
} apt_task_msg_type_t;

/** Task message declaration */
typedef struct apt_task_msg_t apt_task_msg_t;

/** Task message is used for task inter-communication */
struct apt_task_msg_t {
	/** One of apt_task_msg_type_t */
	apt_task_msg_type_t  type;
	/** Context specific data */
	char                 data[1];
};

APT_END_EXTERN_C

#endif /*__APT_TASK_MSG_H__*/
