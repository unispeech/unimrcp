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

#include <stdio.h>
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
	apt_task_vtable_t vtable;
	apt_task_vtable_reset(&vtable);
	vtable.run = sample_task_main;

	if(apr_initialize() != APR_SUCCESS) {
		return 0;
	}

	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	printf("Create Task\n");
	task = apt_task_create(NULL,&vtable,pool);
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
