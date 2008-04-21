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

#ifndef __APT_TASK_H__
#define __APT_TASK_H__

/**
 * @file apt_task.h
 * @brief Thread Execution Abstraction
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Opaque task declaration */
typedef struct apt_task_t apt_task_t;
/** Opaque task virtual table declaration */
typedef struct apt_task_vtable_t apt_task_vtable_t;
/** Opaque task method declaration */
typedef void (*apt_task_method_f)(apt_task_t *task);


/**
 * Create task.
 * @param obj the external object to associate with the task
 * @param vtable the table of virtual methods of the task
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(apt_task_t*) apt_task_create(void *obj, apt_task_vtable_t *vtable, apr_pool_t *pool);

/**
 * Destroy task.
 * @param task the task to destroy
 */
APT_DECLARE(apt_bool_t) apt_task_destroy(apt_task_t *task);

/**
 * Start task.
 * @param task the task to start
 */
APT_DECLARE(apt_bool_t) apt_task_start(apt_task_t *task);

/**
 * Terminate task.
 * @param task the task to terminate
 * @param wait_till_complete whether to wait for task to complete or
 *                           process termination asynchronously
 */
APT_DECLARE(apt_bool_t) apt_task_terminate(apt_task_t *task, apt_bool_t wait_till_complete);

/**
 * Wait for task till complete.
 * @param task the task to wait for
 */
APT_DECLARE(apt_bool_t) apt_task_wait_till_complete(apt_task_t *task);

/**
 * Hold task execution.
 * @param task the task to hold
 * @param msec the time to hold
 */
APT_DECLARE(void) apt_task_delay(apr_size_t msec);

/**
 * Get external object associated with the task.
 * @param task the task to get object from
 */
APT_DECLARE(void*) apt_task_object_get(apt_task_t *task);


/** Table of task virtual methods */
struct apt_task_vtable_t {
	apt_task_method_f destroy;
	apt_task_method_f start;
	apt_task_method_f terminate;
	apt_task_method_f pre_run;
	apt_task_method_f run;
	apt_task_method_f post_run;
};

static APR_INLINE void apt_task_vtable_reset(apt_task_vtable_t *vtable)
{
	vtable->destroy = NULL;
	vtable->start = NULL;
	vtable->terminate = NULL;
	vtable->pre_run = NULL;
	vtable->post_run = NULL;
}

APT_END_EXTERN_C

#endif /*__APT_TASK_H__*/
