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

#include <apr_queue.h>
#include "apt_consumer_task.h"

struct apt_consumer_task_t {
	apt_composite_task_t *base;
	apr_queue_t          *msg_queue;
	void                 *obj;
};

static apt_bool_t apt_consumer_task_msg_signal(apt_composite_task_t *composite_task, apt_task_msg_t *msg);
static void apt_consumer_task_run(apt_task_t *task);

APT_DECLARE(apt_consumer_task_t*) apt_consumer_task_create(
									void *obj,
									apt_task_vtable_t *base_vtable,
									apt_composite_task_vtable_t *vtable,
									apt_task_msg_pool_t *msg_pool,
									apr_pool_t *pool)
{
	apt_consumer_task_t *consumer_task = apr_palloc(pool,sizeof(apt_consumer_task_t));
	consumer_task->msg_queue = NULL;
	if(apr_queue_create(&consumer_task->msg_queue,1024,pool) != APR_SUCCESS) {
		return NULL;
	}
	
	if(base_vtable) {
		base_vtable->run = apt_consumer_task_run;
	}
	if(vtable) {
		vtable->signal_msg = apt_consumer_task_msg_signal;
	}
	consumer_task->base = apt_composite_task_create(consumer_task,base_vtable,vtable,msg_pool,pool);
	consumer_task->obj = obj;
	return consumer_task;
}

APT_DECLARE(apt_composite_task_t*) apt_consumer_task_base_get(apt_consumer_task_t *task)
{
	return task->base;
}

static apt_bool_t apt_consumer_task_msg_signal(apt_composite_task_t *composite_task, apt_task_msg_t *msg)
{
	apt_consumer_task_t *consumer_task = apt_composite_task_object_get(composite_task);
	return (apr_queue_push(consumer_task->msg_queue,msg) == APR_SUCCESS) ? TRUE : FALSE;
}

static void apt_consumer_task_run(apt_task_t *task)
{
	apr_status_t rv;
	void *msg;
	apt_bool_t running = TRUE;
	apt_consumer_task_t *consumer_task;
	apt_composite_task_t *composite_task = apt_task_object_get(task);
	if(!composite_task) {
		return;
	}
	consumer_task = apt_composite_task_object_get(composite_task);
	if(!consumer_task) {
		return;
	}

	while(running) {
		rv = apr_queue_pop(consumer_task->msg_queue,&msg);
		if(rv == APR_SUCCESS) {
			if(msg) {
				apt_task_msg_t *task_msg = msg;
				if(apt_composite_task_msg_process(consumer_task->base,task_msg) == FALSE) {
					running = FALSE;
				}
			}
		}
	}
}
