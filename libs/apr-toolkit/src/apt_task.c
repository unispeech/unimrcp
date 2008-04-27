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

#include <apr_thread_proc.h>
#include <apr_thread_cond.h>
#include "apt_task.h"

/** Internal states of the task */
typedef enum {
	TASK_STATE_IDLE,               /**< no task activity */
	TASK_STATE_START_REQUESTED,    /**< task start is requested and is in progress */
	TASK_STATE_RUNNING,            /**< task is running */
	TASK_STATE_TERMINATE_REQUESTED /**< task termination is requested and is in progress */
} apt_task_state_t;

struct apt_task_t {
	apr_pool_t         *pool;          /* memory pool to allocate task data from */
	apr_thread_mutex_t *data_guard;    /* mutex to protect task data */
	apr_thread_t       *thread_handle; /* thread handle */
	apt_task_state_t    state;         /* current task state */
	apt_task_vtable_t   vtable;        /* table of virtual methods */
	void               *obj;           /* external object associated with the task */
};


static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data);

static APR_INLINE void apt_task_vtable_copy(apt_task_t *task, const apt_task_vtable_t *vtable)
{
	if(vtable->destroy) {
		task->vtable.destroy = vtable->destroy;
	}
	if(vtable->start) {
		task->vtable.start = vtable->start;
	}
	if(vtable->terminate) {
		task->vtable.terminate = vtable->terminate;
	}
	if(vtable->pre_run) {
		task->vtable.pre_run = vtable->pre_run;
	}
	if(vtable->run) {
		task->vtable.run = vtable->run;
	}
	if(vtable->post_run) {
		task->vtable.post_run = vtable->post_run;
	}
}


APT_DECLARE(apt_task_t*) apt_task_create(void *obj, apt_task_vtable_t *vtable, apr_pool_t *pool)
{
	apt_task_t *task = apr_palloc(pool,sizeof(apt_task_t));
	task->pool = pool;

	task->state = TASK_STATE_IDLE;
	task->thread_handle = NULL;
	if(apr_thread_mutex_create(&task->data_guard, APR_THREAD_MUTEX_DEFAULT, task->pool) != APR_SUCCESS) {
		return NULL;
	}
	task->obj = obj;
	apt_task_vtable_reset(&task->vtable);
	if(vtable) {
		apt_task_vtable_copy(task,vtable);
	}
	return task;
}

APT_DECLARE(apt_bool_t) apt_task_destroy(apt_task_t *task)
{
	if(task->state != TASK_STATE_IDLE) {
		apt_task_wait_till_complete(task);
	}

	if(task->vtable.destroy) {
		task->vtable.destroy(task);
	}
	
	apr_thread_mutex_destroy(task->data_guard);
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_start(apt_task_t *task)
{
	apt_bool_t status = TRUE;
	apr_thread_mutex_lock(task->data_guard);
	if(task->state == TASK_STATE_IDLE) {
		apr_status_t rv;
		task->state = TASK_STATE_START_REQUESTED;
		/* raise start request event */
		if(task->vtable.start) {
			task->vtable.start(task);
		}
		rv = apr_thread_create(&task->thread_handle,NULL,apt_task_run,task,task->pool);
		if(rv != APR_SUCCESS) {
			task->state = TASK_STATE_IDLE;
			status = FALSE;
		}
	}
	else {
		status = FALSE;
	}
	apr_thread_mutex_unlock(task->data_guard);
	return status;
}

APT_DECLARE(apt_bool_t) apt_task_terminate(apt_task_t *task, apt_bool_t wait_till_complete)
{
	apr_thread_mutex_lock(task->data_guard);
	if(task->state == TASK_STATE_START_REQUESTED || task->state == TASK_STATE_RUNNING) {
		task->state = TASK_STATE_TERMINATE_REQUESTED;
	}
	apr_thread_mutex_unlock(task->data_guard);

	if(task->state == TASK_STATE_TERMINATE_REQUESTED) {
		/* raise terminate request event */
		if(task->vtable.terminate) {
			task->vtable.terminate(task);
		}

		if(wait_till_complete == TRUE) {
			apt_task_wait_till_complete(task);
		}
	}

	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_task_wait_till_complete(apt_task_t *task)
{
	if(task->thread_handle) {
		apr_status_t s;
		apr_thread_join(&s,task->thread_handle);
		task->thread_handle = NULL;
	}
	return TRUE;
}

APT_DECLARE(void) apt_task_delay(apr_size_t msec)
{
	apr_sleep(1000*msec);
}

APT_DECLARE(void*) apt_task_object_get(apt_task_t *task)
{
	return task->obj;
}

static void* APR_THREAD_FUNC apt_task_run(apr_thread_t *thread_handle, void *data)
{
	apt_task_t *task = data;
	
	/* raise pre-run event */
	if(task->vtable.pre_run) {
		task->vtable.pre_run(task);
	}
	apr_thread_mutex_lock(task->data_guard);
	task->state = TASK_STATE_RUNNING;
	apr_thread_mutex_unlock(task->data_guard);

	/* run task */
	if(task->vtable.run) {
		task->vtable.run(task);
	}

	apr_thread_mutex_lock(task->data_guard);
	task->state = TASK_STATE_IDLE;
	apr_thread_mutex_unlock(task->data_guard);
	/* raise post-run event */
	if(task->vtable.post_run) {
		task->vtable.post_run(task);
	}
	return NULL;
}
