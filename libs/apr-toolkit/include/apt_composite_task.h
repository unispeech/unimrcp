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

#ifndef __APT_COMPOSITE_TASK_H__
#define __APT_COMPOSITE_TASK_H__

/**
 * @file apt_composite_task.h
 * @brief Composition of Tasks, Inter-task Communication, Master and Slave Tasks
 */ 

#include "apt_task.h"
#include "apt_task_msg.h"

APT_BEGIN_EXTERN_C

/** Opaque composite task declaration */
typedef struct apt_composite_task_t apt_composite_task_t;
/** Opaque virtual table declaration */
typedef struct apt_composite_task_vtable_t apt_composite_task_vtable_t;


/**
 * Create composite task.
 * @param obj the external object to associate with the task
 * @param vtable the table of virtual methods of the task
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_composite_task_t*) apt_composite_task_create(
									void *obj,
									apt_task_vtable_t *base_vtable,
									apt_composite_task_vtable_t *vtable,
									apr_pool_t *pool);

/**
 * Add slave task.
 * @param master_task the master task to add slave task to
 * @param slave_task the slave task to add
 */
APT_DECLARE(apt_bool_t) apt_composite_task_add(apt_composite_task_t *master_task, apt_composite_task_t *slave_task);

/**
 * Signal (post) message to the task.
 * @param task the composite task to signal message to
 * @param msg the message to signal
 */
APT_DECLARE(apt_bool_t) apt_composite_task_msg_signal(apt_composite_task_t *task, apt_task_msg_t *msg);

/**
 * Process message signaled to the task.
 * @param task the composite task to process message
 * @param msg the message to process
 */
APT_DECLARE(apt_bool_t) apt_composite_task_msg_process(apt_composite_task_t *task, apt_task_msg_t *msg);

/**
 * Get base task.
 * @param task the composite task to get base for
 */
APT_DECLARE(apt_task_t*) apt_composite_task_base_get(apt_composite_task_t *task);

/**
 * Get external object associated with the composite task.
 * @param task the composite task to get object from
 */
APT_DECLARE(void*) apt_composite_task_object_get(apt_composite_task_t *task);


/** Table of composite task virtual methods */
struct apt_composite_task_vtable_t {
	apt_bool_t (*signal_msg)(apt_composite_task_t *task, apt_task_msg_t *msg);
	apt_bool_t (*process_msg)(apt_composite_task_t *task, apt_task_msg_t *msg);

	apt_bool_t (*on_start_complete)(apt_composite_task_t *task);
	apt_bool_t (*on_terminate_complete)(apt_composite_task_t *task);
};

static APR_INLINE void apt_composite_task_vtable_reset(apt_composite_task_vtable_t *vtable)
{
	vtable->signal_msg = NULL;
	vtable->process_msg = NULL;
	vtable->on_start_complete = NULL;
	vtable->on_terminate_complete = NULL;
}

APT_END_EXTERN_C

#endif /*__APT_COMPOSITE_TASK_H__*/
