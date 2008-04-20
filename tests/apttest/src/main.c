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

#include <stdlib.h>
#include "apt_task.h"

static void sample_task_main(void *obj)
{
	printf("Do the Job\n");
	apt_task_delay(5000);
}

int main(int argc, char *argv[])
{
	apr_pool_t *pool;
	apt_task_t *task;
	/* simply declare const task vtable */
	const apt_task_vtable_t sample_task_vtable = {
		sample_task_main,
		NULL,
		NULL,
		NULL,
		NULL
	};

#if 0
	/* another way of task vtable usage */
	apt_task_vtable_t another_task_vtable;
	apt_task_vtable_reset(&another_task_vtable);
	another_task_vtable.main = sample_task_main;
#endif

	if(apr_initialize() != APR_SUCCESS) {
		return 0;
	}

	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	printf("Create Task\n");
	task = apt_task_create(NULL,&sample_task_vtable,pool);
	printf("Start Task\n");
	apt_task_start(task);
	printf("Wait for Task to Complete\n");
	apt_task_wait_till_complete(task);
	printf("Destroy Task\n");
	apt_task_destroy(task);

	apr_pool_destroy(pool);
	apr_terminate();
	return 0;
}
