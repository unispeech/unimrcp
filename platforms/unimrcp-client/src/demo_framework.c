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

#include "demo_framework.h"
#include "demo_application.h"
#include "unimrcp_client.h"
#include "apt_consumer_task.h"

/** Demo framework */
struct demo_framework_t {
	/** MRCP client stack instance */
	mrcp_client_t       *client;
	/** Message processing task */
	apt_consumer_task_t *task;
};

typedef struct framework_task_data_t framework_task_data_t;
struct framework_task_data_t {
	demo_framework_t         *framework;
	mrcp_application_event_t *app_event;
};

static apt_bool_t demo_framework_event_handler(const mrcp_application_event_t *app_event);
static apt_consumer_task_t* demo_framework_consumer_task_create(apr_pool_t *pool);

/** Create demo framework */
demo_framework_t* demo_framework_create()
{
	demo_framework_t *framework = NULL;
	mrcp_client_t *client = unimrcp_client_create();
	if(client) {
		demo_application_t *demo_application;
		apr_pool_t *pool = mrcp_client_memory_pool_get(client);
		framework = apr_palloc(pool,sizeof(demo_framework_t));
		framework->client = client;
		framework->task = demo_framework_consumer_task_create(pool);

		demo_application = demo_synth_application_create(pool);
		if(demo_application) {
			demo_application->application = mrcp_application_create(framework,demo_framework_event_handler,pool);
			demo_application->framework = framework;
			mrcp_client_application_register(client,demo_application->application);
		}

		if(framework->task) {
			apt_task_t *task = apt_consumer_task_base_get(framework->task);
			apt_task_start(task);
		}
		 
		mrcp_client_start(client);
	}

	return framework;
}

/** Destroy demo framework */
apt_bool_t demo_framework_destroy(demo_framework_t *framework)
{
	if(!framework) {
		return FALSE;
	}

	if(framework->task) {
		apt_task_t *task = apt_consumer_task_base_get(framework->task);
		apt_task_terminate(task,TRUE);
		apt_task_destroy(task);
		framework->task = NULL;
	}

	mrcp_client_shutdown(framework->client);
	return mrcp_client_destroy(framework->client);
}

static void demo_framework_on_start_complete(apt_task_t *task)
{
}

static void demo_framework_on_terminate_complete(apt_task_t *task)
{
}

static apt_bool_t demo_framework_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	return TRUE;
}

static apt_consumer_task_t* demo_framework_consumer_task_create(apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	apt_consumer_task_t *task;

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = demo_framework_msg_process;
	vtable.on_start_complete = demo_framework_on_start_complete;
	vtable.on_terminate_complete = demo_framework_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);
	task = apt_consumer_task_create(NULL, &vtable, msg_pool, pool);
	return task;
}

static apt_bool_t demo_framework_event_handler(const mrcp_application_event_t *app_event)
{
	return TRUE;
}
