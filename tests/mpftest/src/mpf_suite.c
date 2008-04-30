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
#include "apt_log.h"

static apt_bool_t mpf_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	apt_task_t *task;
	mpf_engine_t *engine;
	mpf_context_t *context;
	mpf_termination_t *termination1;
	mpf_termination_t *termination2;
	apt_task_msg_t *msg;
	mpf_message_t *mpf_message;
	
	apt_log(APT_PRIO_NOTICE,"Create MPF Engine");
	engine = mpf_engine_create(suite->pool);
	if(!engine) {
		apt_log(APT_PRIO_WARNING,"Failed to Create MPF Engine");
		return FALSE;
	}
	task = mpf_task_get(engine);

	apt_log(APT_PRIO_INFO,"Start Task");
	if(apt_task_start(task) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Start Task");
		apt_task_destroy(task);
		return FALSE;
	}

	apt_log(APT_PRIO_INFO,"Create MPF Context");
	context = mpf_context_create(NULL,suite->pool);

	apt_log(APT_PRIO_INFO,"Create Termination [1]");
	termination1 = mpf_termination_create(NULL,suite->pool);

	apt_log(APT_PRIO_INFO,"Add Termination [1] to Context");
	msg = apt_task_msg_get(task);
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->action_type = MPF_ACTION_TYPE_CONTEXT;
	mpf_message->action_id = MPF_CONTEXT_ACTION_ADD;
	mpf_message->context = context;
	mpf_message->termination = termination1;
	apt_task_msg_signal(task,msg);

	apt_log(APT_PRIO_INFO,"Create Termination [2]");
	termination2 = mpf_termination_create(NULL,suite->pool);

	apt_log(APT_PRIO_INFO,"Add Termination [2] to Context");
	msg = apt_task_msg_get(task);
	mpf_message = (mpf_message_t*) msg->data;

	mpf_message->message_type = MPF_MESSAGE_TYPE_REQUEST;
	mpf_message->action_type = MPF_ACTION_TYPE_CONTEXT;
	mpf_message->action_id = MPF_CONTEXT_ACTION_ADD;
	mpf_message->context = context;
	mpf_message->termination = termination2;
	apt_task_msg_signal(task,msg);

	apt_task_delay(5000);

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
