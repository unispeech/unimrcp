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
#include "apt_obj_list.h"
#include "apt_log.h"

#define MAX_APP_NAME_LENGTH     16
#define MAX_PROFILE_NAME_LENGTH 16

/** Demo framework */
struct demo_framework_t {
	/** MRCP client stack instance */
	mrcp_client_t       *client;
	/** Message processing task */
	apt_consumer_task_t *task;
	/** List of available demo applications */
	apt_obj_list_t      *application_list;
	/** Memory to allocate memory from */
	apr_pool_t          *pool;
};

typedef struct framework_task_data_t framework_task_data_t;
struct framework_task_data_t {
	char                      app_name[MAX_APP_NAME_LENGTH];
	char                      profile_name[MAX_PROFILE_NAME_LENGTH];
	demo_application_t       *demo_application;
	const mrcp_app_message_t *app_message;
};

typedef enum {
	DEMO_APPLICATION_MSG_ID,
	DEMO_CONSOLE_MSG_ID
} framework_msg_type_e;

static apt_bool_t demo_framework_event_handler(const mrcp_app_message_t *app_message);
static apt_bool_t demo_framework_consumer_task_create(demo_framework_t *framework);

/** Create demo framework */
demo_framework_t* demo_framework_create(const char *conf_file_path)
{
	demo_framework_t *framework = NULL;
	mrcp_client_t *client = unimrcp_client_create(conf_file_path);
	if(client) {
		apr_pool_t *pool = mrcp_client_memory_pool_get(client);
		framework = apr_palloc(pool,sizeof(demo_framework_t));
		framework->pool = pool;
		framework->client = client;
		framework->application_list = apt_list_create(pool);
		
		demo_framework_consumer_task_create(framework);

		if(framework->task) {
			apt_task_t *task = apt_consumer_task_base_get(framework->task);
			apt_task_start(task);
		}
		 
		mrcp_client_start(client);
	}

	return framework;
}

/** Run demo application */
apt_bool_t demo_framework_app_run(demo_framework_t *framework, const char *app_name, const char *profile_name)
{
	apt_task_t *task = apt_consumer_task_base_get(framework->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		framework_task_data_t *framework_task_data = (framework_task_data_t*)task_msg->data;
		task_msg->type = TASK_MSG_USER;
		task_msg->sub_type = DEMO_CONSOLE_MSG_ID;
		framework_task_data = (framework_task_data_t*) task_msg->data;
		strcpy(framework_task_data->app_name,app_name);
		strcpy(framework_task_data->profile_name,profile_name);
		framework_task_data->app_message = NULL;
		framework_task_data->demo_application = NULL;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
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

	if(framework->application_list) {
		apt_list_destroy(framework->application_list);
		framework->application_list = NULL;
	}

	mrcp_client_shutdown(framework->client);
	return mrcp_client_destroy(framework->client);
}

static void demo_framework_on_start_complete(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	demo_framework_t *framework = apt_consumer_task_object_get(consumer_task);
	apt_log(APT_PRIO_NOTICE,"Run Demo Framework");
	if(framework) {
		demo_application_t *demo_application;
		demo_application = demo_synth_application_create(framework->pool);
		if(demo_application) {
			apt_list_push_back(framework->application_list,demo_application);
		}

		demo_application = demo_recog_application_create(framework->pool);
		if(demo_application) {
			apt_list_push_back(framework->application_list,demo_application);
		}
	}
}

static void demo_framework_on_terminate_complete(apt_task_t *task)
{
}

static void demo_framework_app_do_run(demo_framework_t *framework, const char *app_name, const char *profile_name)
{
	demo_application_t *demo_application;
	apt_list_elem_t *elem = apt_list_first_elem_get(framework->application_list);
	/* walk through the list of the applications */
	while(elem) {
		demo_application = apt_list_elem_object_get(elem);
		if(demo_application && strcasecmp(demo_application->name,app_name) == 0) {
			apt_log(APT_PRIO_NOTICE,"Run Demo Application [%s]",app_name);
			if(!demo_application->application) {
				demo_application->application = mrcp_application_create(
													demo_framework_event_handler,
													demo_application,
													framework->pool);
				demo_application->framework = framework;
				mrcp_client_application_register(framework->client,demo_application->application);
			}

			demo_application->vtable->run(demo_application,profile_name);
			return;
		}
		elem = apt_list_next_elem_get(framework->application_list,elem);
	}
	apt_log(APT_PRIO_WARNING,"No Such Demo Application [%s]",app_name);
}

static apt_bool_t demo_framework_response_process(demo_application_t *demo_application, const mrcp_app_message_t *app_message)
{
	switch(app_message->command_id) {
		case MRCP_APP_COMMAND_SESSION_UPDATE:
			demo_application->vtable->on_session_update(demo_application,app_message->session);
			break;
		case MRCP_APP_COMMAND_SESSION_TERMINATE:
			demo_application->vtable->on_session_terminate(demo_application,app_message->session);
			break;
		case MRCP_APP_COMMAND_CHANNEL_ADD:
			demo_application->vtable->on_channel_add(
					demo_application,
					app_message->session,
					app_message->channel,
					app_message->descriptor);
			break;
		case MRCP_APP_COMMAND_CHANNEL_REMOVE:
			demo_application->vtable->on_channel_remove(
					demo_application,
					app_message->session,
					app_message->channel);
			break;
		case MRCP_APP_COMMAND_MESSAGE:
			demo_application->vtable->on_message_receive(
					demo_application,
					app_message->session,
					app_message->channel,
					app_message->mrcp_message);
			break;
		default:
			break;
	}
	return TRUE;
}

static apt_bool_t demo_framework_event_process(demo_application_t *demo_application, const mrcp_app_message_t *app_message)
{
	demo_application->vtable->on_message_receive(
			demo_application,
			app_message->session,
			app_message->channel,
			app_message->mrcp_message);
	return TRUE;
}

static apt_bool_t demo_framework_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	if(msg->type == TASK_MSG_USER) {
		switch(msg->sub_type) {
			case DEMO_APPLICATION_MSG_ID:
			{
				framework_task_data_t *framework_task_data = (framework_task_data_t*)msg->data;
				const mrcp_app_message_t *app_message = framework_task_data->app_message;
				switch(app_message->message_type) {
					case MRCP_APP_MESSAGE_TYPE_RESPONSE:
						demo_framework_response_process(framework_task_data->demo_application,app_message);
						break;
					case MRCP_APP_MESSAGE_TYPE_EVENT:
						demo_framework_event_process(framework_task_data->demo_application,app_message);
						break;
					default:
						break;
				}
				break;
			}
			case DEMO_CONSOLE_MSG_ID:
			{
				framework_task_data_t *framework_task_data = (framework_task_data_t*)msg->data;
				apt_consumer_task_t *consumer_task = apt_task_object_get(task);
				demo_framework_t *framework = apt_consumer_task_object_get(consumer_task);
				demo_framework_app_do_run(
							framework,
							framework_task_data->app_name,
							framework_task_data->profile_name);
				break;
			}
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

static apt_bool_t demo_framework_event_handler(const mrcp_app_message_t *app_message)
{
	demo_application_t *demo_application;
	if(!app_message->application) {
		return FALSE;
	}
	demo_application = mrcp_application_object_get(app_message->application);
	if(demo_application && demo_application->framework) {
		demo_framework_t *framework = demo_application->framework;
		apt_task_t *task = apt_consumer_task_base_get(framework->task);
		apt_task_msg_t *task_msg = apt_task_msg_get(task);
		if(task_msg) {
			framework_task_data_t *framework_task_data = (framework_task_data_t*)task_msg->data;
			task_msg->type = TASK_MSG_USER;
			task_msg->sub_type = DEMO_APPLICATION_MSG_ID;
			framework_task_data = (framework_task_data_t*) task_msg->data;
			framework_task_data->app_message = app_message;
			framework_task_data->demo_application = demo_application;
			apt_task_msg_signal(task,task_msg);
		}
	}
	return TRUE;
}
