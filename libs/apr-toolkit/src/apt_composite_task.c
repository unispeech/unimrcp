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

#include "apt_composite_task.h"
#include "apt_obj_list.h"

struct apt_composite_task_t {
	apt_task_t                 *base;              /* base task */
	apt_composite_task_vtable_t vtable;            /* table of virtual methods */
	void                       *obj;               /* external object associated with the task */

	apt_composite_task_t       *master_task;       /* master task */
	apt_obj_list_t             *slave_tasks;       /* list of the slave tasks */

	apr_size_t                  pending_start;     /* number of pending start requests */
	apr_size_t                  pending_terminate; /* number of pending terminate requests */
};

static void apt_task_on_terminate(apt_task_t *task);
static void apt_task_on_pre_run(apt_task_t *task);

static APR_INLINE void apt_composite_task_vtable_copy(apt_composite_task_t *task, const apt_composite_task_vtable_t *vtable)
{
	if(vtable->signal_msg) {
		task->vtable.signal_msg = vtable->signal_msg;
	}
	if(vtable->process_msg) {
		task->vtable.process_msg = vtable->process_msg;
	}
	if(vtable->on_start_complete) {
		task->vtable.on_start_complete = vtable->on_start_complete;
	}
	if(vtable->on_terminate_complete) {
		task->vtable.on_terminate_complete = vtable->on_terminate_complete;
	}
}


APT_DECLARE(apt_composite_task_t*) apt_composite_task_create(
									void *obj,
									apt_task_vtable_t *base_vtable,
									apt_composite_task_vtable_t *vtable,
									apr_pool_t *pool)
{
	apt_composite_task_t *composite_task = apr_palloc(pool,sizeof(apt_composite_task_t));
	if(base_vtable) {
		if(!base_vtable->terminate) {
			base_vtable->terminate = apt_task_on_terminate;
		}
		if(!base_vtable->pre_run) {
			base_vtable->pre_run = apt_task_on_pre_run;
		}
	}
	composite_task->base = apt_task_create(composite_task,base_vtable,pool);
	composite_task->obj = obj;
	apt_composite_task_vtable_reset(&composite_task->vtable);
	if(vtable) {
		apt_composite_task_vtable_copy(composite_task,vtable);
	}
	composite_task->slave_tasks = apt_list_create(pool);
	composite_task->master_task = NULL;
	composite_task->pending_start = 0;
	composite_task->pending_terminate = 0;
	return composite_task;
}

APT_DECLARE(apt_bool_t) apt_composite_task_add(apt_composite_task_t *master_task, apt_composite_task_t *slave_task)
{
	apt_list_push_back(master_task->slave_tasks,slave_task);
	slave_task->master_task = master_task;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_composite_task_msg_signal(apt_composite_task_t *task, apt_task_msg_t *msg)
{
	if(task->vtable.signal_msg) {
		return task->vtable.signal_msg(task,msg);
	}
	return FALSE;
}

APT_DECLARE(apt_bool_t) apt_composite_task_msg_process(apt_composite_task_t *task, apt_task_msg_t *msg)
{
	switch(msg->type) {
		case TASK_MSG_START_COMPLETE: 
		{
			if(task->pending_start) {
				task->pending_start--;
				if(!task->pending_start) {
					if(task->vtable.on_start_complete) {
						task->vtable.on_start_complete(task);
					}
					if(task->master_task) {
						/* signal start-complete message */
						apt_composite_task_msg_signal(task->master_task,msg);
					}
				}
			}
			break;
		}
		case TASK_MSG_TERMINATE_REQUEST:
		{
			apt_list_elem_t *elem = apt_list_first_elem_get(task->slave_tasks);
			apt_composite_task_t *slave_task = NULL;
			task->pending_terminate = 0;
			/* walk through the list of the slave tasks and terminate them */
			while(elem) {
				slave_task = apt_list_elem_object_get(elem);
				if(slave_task) {
					if(apt_task_terminate(slave_task->base,FALSE) == TRUE) {
						task->pending_terminate++;
					}
				}
				elem = apt_list_next_elem_get(task->slave_tasks,elem);
			}

			if(!task->pending_terminate) {
				/* no slave task to terminate, just raise on_terminate_complete event */
				if(task->vtable.on_terminate_complete) {
					task->vtable.on_terminate_complete(task);
				}
				if(task->master_task) {
					/* signal terminate-complete message */
					apt_composite_task_msg_signal(task->master_task,msg);
				}
			}
			break;
		}
		case TASK_MSG_TERMINATE_COMPLETE:
		{
			if(task->pending_terminate) {
				task->pending_terminate--;
				if(!task->pending_terminate) {
					if(task->vtable.on_terminate_complete) {
						task->vtable.on_terminate_complete(task);
					}
					if(task->master_task) {
						/* signal terminate-complete message */
						apt_composite_task_msg_signal(task->master_task,msg);
					}
				}
			}
			break;
		}
		default: 
		{
			if(task->vtable.process_msg) {
				task->vtable.process_msg(task,msg);
			}
		}
	}

	return TRUE;
}

APT_DECLARE(apt_task_t*) apt_composite_task_base_get(apt_composite_task_t *task)
{
	return task->base;
}

APT_DECLARE(void*) apt_composite_task_object_get(apt_composite_task_t *task)
{
	return task->obj;
}

static void apt_task_on_terminate(apt_task_t *task)
{
	apt_composite_task_t *composite_task = apt_task_object_get(task);
	apt_task_msg_t msg;
	/* signal terminate-request message */
	msg.type = TASK_MSG_TERMINATE_REQUEST;
	apt_composite_task_msg_signal(composite_task,&msg);
}

static void apt_task_on_pre_run(apt_task_t *task)
{
	apt_composite_task_t *composite_task = apt_task_object_get(task);
	apt_list_elem_t *elem = apt_list_first_elem_get(composite_task->slave_tasks);
	apt_composite_task_t *slave_task = NULL;
	composite_task->pending_start = 0;
	/* walk through the list of the slave tasks and start them */
	while(elem) {
		slave_task = apt_list_elem_object_get(elem);
		if(slave_task) {
			if(apt_task_start(slave_task->base) == TRUE) {
				composite_task->pending_start++;
			}
		}
		elem = apt_list_next_elem_get(composite_task->slave_tasks,elem);
	}

	if(!composite_task->pending_start) {
		/* no slave task to start, just raise on_start_complete event */
		if(composite_task->vtable.on_start_complete) {
			composite_task->vtable.on_start_complete(composite_task);
		}
		if(composite_task->master_task) {
			/* signal start-complete message */
			apt_task_msg_t msg;
			msg.type = TASK_MSG_START_COMPLETE;
			apt_composite_task_msg_signal(composite_task->master_task,&msg);
		}
	}
}
