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

#include "apt_test_suite.h"
#include "mpf_engine.h"
#include "mpf_user.h"
#include "mpf_audio_file_stream.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

static void task_on_start_complete(apt_task_t *task)
{
	apt_task_t *consumer_task;
	apt_task_t *engine_task;
	mpf_context_t *context;
	mpf_audio_stream_t *audio_stream;
	mpf_termination_t *termination1;
	mpf_termination_t *termination2;
	apt_task_msg_t *msg;
	mpf_message_t *mpf_message;
	apr_pool_t *pool;

	apt_log(APT_PRIO_INFO,"On Task Start");
	consumer_task = apt_task_object_get(task);
	engine_task = apt_task_object_get(consumer_task);
	pool = apt_task_pool_get(task);

	apt_log(APT_PRIO_INFO,"Create MPF Context");
	context = mpf_context_create(NULL,pool);

	audio_stream = mpf_audio_file_reader_create("demo.pcm",pool);
	
	apt_log(APT_PRIO_INFO,"Create Termination [1]");
	termination1 = mpf_termination_create(NULL,NULL,audio_stream,NULL,pool);

	apt_log(APT_PRIO_INFO,"Add Termination [1] to Context");
	msg = apt_task_msg_get(task);
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = MPF_COMMAND_ADD;
	mpf_message->context = context;
	mpf_message->termination = termination1;
	apt_task_msg_signal(engine_task,msg);

	audio_stream = mpf_audio_file_writer_create("demo_out.pcm",pool);

	apt_log(APT_PRIO_INFO,"Create Termination [2]");
	termination2 = mpf_termination_create(NULL,NULL,audio_stream,NULL,pool);

	apt_log(APT_PRIO_INFO,"Add Termination [2] to Context");
	msg = apt_task_msg_get(task);
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->command_id = MPF_COMMAND_ADD;
	mpf_message->context = context;
	mpf_message->termination = termination2;
	apt_task_msg_signal(engine_task,msg);
}

static void task_on_terminate_complete(apt_task_t *task)
{
	apt_log(APT_PRIO_INFO,"On Task Terminate");
}

static apt_bool_t task_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	apt_log(APT_PRIO_DEBUG,"Process MPF Response");
	return TRUE;
}

static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mpf_engine_t *engine;
	apt_task_t *engine_task;

	apt_consumer_task_t *consumer_task;
	apt_task_t *task;
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;

	engine = mpf_engine_create(suite->pool);
	if(!engine) {
		apt_log(APT_PRIO_WARNING,"Failed to Create MPF Engine");
		return FALSE;
	}
	engine_task = mpf_task_get(engine);

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = task_msg_process;
	vtable.on_start_complete = task_on_start_complete;
	vtable.on_terminate_complete = task_on_terminate_complete;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),suite->pool);

	apt_log(APT_PRIO_NOTICE,"Create Consumer Task");
	consumer_task = apt_consumer_task_create(engine_task,&vtable,msg_pool,suite->pool);
	if(!consumer_task) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Consumer Task");
		return FALSE;
	}
	task = apt_consumer_task_base_get(consumer_task);

	apt_task_add(task,engine_task);

	apt_log(APT_PRIO_INFO,"Start Task");
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	apt_log(APT_PRIO_INFO,"Press Enter to Exit");
	getchar();
	
	apt_log(APT_PRIO_INFO,"Terminate Task [wait till complete]");
	apt_task_terminate(task,TRUE);
	apt_log(APT_PRIO_NOTICE,"Destroy Task");
	apt_task_destroy(task);
	return TRUE;
}

apt_test_suite_t* mpf_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"mpf",NULL,mpf_test_run);
	return suite;
}
