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
	/** Memory to allocate memory from */
	apr_pool_t          *pool;
};

typedef struct framework_task_data_t framework_task_data_t;
struct framework_task_data_t {
	demo_application_t             *demo_application;
	const mrcp_application_event_t *app_event;
};

static apt_bool_t demo_framework_event_handler(const mrcp_application_event_t *app_event);
static apt_bool_t demo_framework_consumer_task_create(demo_framework_t *framework);

/** Create demo framework */
demo_framework_t* demo_framework_create()
{
	demo_framework_t *framework = NULL;
	mrcp_client_t *client = unimrcp_client_create();
	if(client) {
		apr_pool_t *pool = mrcp_client_memory_pool_get(client);
		framework = apr_palloc(pool,sizeof(demo_framework_t));
		framework->pool = pool;
		framework->client = client;
		
		demo_framework_consumer_task_create(framework);

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
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	demo_framework_t *framework = apt_consumer_task_object_get(consumer_task);
	if(framework) {
		demo_application_t *demo_application;
		demo_application = demo_synth_application_create(framework->pool);
		if(demo_application) {
			demo_application->application = mrcp_application_create(
				demo_application,demo_framework_event_handler,framework->pool);
			demo_application->framework = framework;
			mrcp_client_application_register(framework->client,demo_application->application);

			demo_application->vtable->run(demo_application);
		}
	}
}

static void demo_framework_on_terminate_complete(apt_task_t *task)
{
}

static apt_bool_t demo_framework_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	if(msg->type == TASK_MSG_USER) {
		framework_task_data_t *framework_task_data = (framework_task_data_t*)msg->data;
		demo_application_t *demo_application = framework_task_data->demo_application;
		switch(framework_task_data->app_event->type) {
			case MRCP_APPLICATION_EVENT_SESSION_UPDATE:
				demo_application->vtable->on_session_update(demo_application,framework_task_data->app_event->session);
				break;
			case MRCP_APPLICATION_EVENT_SESSION_TERMINATE:
				demo_application->vtable->on_session_terminate(demo_application,framework_task_data->app_event->session);
				break;
			case MRCP_APPLICATION_EVENT_CHANNEL_MODIFY:
				demo_application->vtable->on_channel_modify(
						demo_application,
						framework_task_data->app_event->session,
						framework_task_data->app_event->channel,
						framework_task_data->app_event->descriptor);
				break;
			case MRCP_APPLICATION_EVENT_CHANNEL_REMOVE:
				demo_application->vtable->on_channel_remove(
						demo_application,
						framework_task_data->app_event->session,
						framework_task_data->app_event->channel);
				break;
			case MRCP_APPLICATION_EVENT_MESSAGE_RECEIVE:
				demo_application->vtable->on_message_receive(
						demo_application,
						framework_task_data->app_event->session,
						framework_task_data->app_event->channel,
						framework_task_data->app_event->message);
				break;
			default:
				break;
		}
	}
	return TRUE;
}

static apt_bool_t demo_framework_consumer_task_create(demo_framework_t *framework)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = demo_framework_msg_process;
	vtable.on_start_complete = demo_framework_on_start_complete;
	vtable.on_terminate_complete = demo_framework_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(framework_task_data_t),framework->pool);
	framework->task = apt_consumer_task_create(framework, &vtable, msg_pool, framework->pool);
	return TRUE;
}

static apt_bool_t demo_framework_event_handler(const mrcp_application_event_t *app_event)
{
	demo_application_t *demo_application;
	if(!app_event->application) {
		return FALSE;
	}
	demo_application = mrcp_application_object_get(app_event->application);
	if(demo_application && demo_application->framework) {
		demo_framework_t *framework = demo_application->framework;
		apt_task_t *task = apt_consumer_task_base_get(framework->task);
		apt_task_msg_t *task_msg = apt_task_msg_get(task);
		if(task_msg) {
			framework_task_data_t *framework_task_data = (framework_task_data_t*)task_msg->data;
			task_msg->type = TASK_MSG_USER;
			task_msg->sub_type = 0;
			framework_task_data = (framework_task_data_t*) task_msg->data;
			framework_task_data->app_event = app_event;
			framework_task_data->demo_application = demo_application;
			apt_task_msg_signal(task,task_msg);
		}
	}
	return TRUE;
}
