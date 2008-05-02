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

#include "mpf_engine.h"
#include "mpf_timer.h"
#include "mpf_codec_descriptor.h"
#include "apt_obj_list.h"
#include "apt_log.h"

struct mpf_engine_t {
	apt_task_t         *base;
	apr_pool_t         *pool;
	apr_thread_mutex_t *request_queue_guard;
	apt_obj_list_t     *request_queue;
	apt_obj_list_t     *contexts;
	mpf_timer_t        *timer;
};

static void mpf_engine_main(mpf_timer_t *timer, void *data);
static apt_bool_t mpf_engine_start(apt_task_t *task);
static apt_bool_t mpf_engine_terminate(apt_task_t *task);
static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t mpf_engine_contexts_destroy(mpf_engine_t *engine);

MPF_DECLARE(mpf_engine_t*) mpf_engine_create(apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	mpf_engine_t *engine = apr_palloc(pool,sizeof(mpf_engine_t));
	engine->pool = pool;
	engine->request_queue = NULL;
	engine->contexts = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.start = mpf_engine_start;
	vtable.terminate = mpf_engine_terminate;
	vtable.signal_msg = mpf_engine_msg_signal;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),pool);

	apt_log(APT_PRIO_NOTICE,"Create Media Processing Engine");
	engine->base = apt_task_create(engine,&vtable,msg_pool,pool);
	return engine;
}

MPF_DECLARE(apt_task_t*) mpf_task_get(mpf_engine_t *engine)
{
	return engine->base;
}

static apt_bool_t mpf_engine_start(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	engine->request_queue = apt_list_create(engine->pool);
	apr_thread_mutex_create(&engine->request_queue_guard,APR_THREAD_MUTEX_UNNESTED,engine->pool);

	engine->contexts = apt_list_create(engine->pool);

	apt_log(APT_PRIO_INFO,"Start Media Processing Engine");
	engine->timer = mpf_timer_start(CODEC_FRAME_TIME_BASE,mpf_engine_main,engine,engine->pool);
	apt_task_child_start(task);
	return TRUE;
}

static apt_bool_t mpf_engine_terminate(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	apt_log(APT_PRIO_INFO,"Terminate Media Processing Engine");
	mpf_timer_stop(engine->timer);
	mpf_engine_contexts_destroy(engine);

	apt_task_child_terminate(task);

	apt_list_destroy(engine->contexts);

	apt_list_destroy(engine->request_queue);
	apr_thread_mutex_destroy(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_contexts_destroy(mpf_engine_t *engine)
{
	mpf_context_t *context;
	context = apt_list_pop_front(engine->contexts);
	while(context) {
		mpf_context_destroy(context);
		
		context = apt_list_pop_front(engine->contexts);
	}
	return TRUE;
}

static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg)
{
	mpf_engine_t *engine = apt_task_object_get(task);
	
	apr_thread_mutex_lock(engine->request_queue_guard);
	apt_list_push_back(engine->request_queue,msg);
	apr_thread_mutex_unlock(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_msg_process(mpf_engine_t *engine, const apt_task_msg_t *msg)
{
	apt_task_t *parent_task;
	apt_task_msg_t *response_msg;
	mpf_message_t *response;
	const mpf_message_t *request = (const mpf_message_t*) msg->data;
	apt_log(APT_PRIO_DEBUG,"Process MPF Message");
	if(request->message_type != MPF_MESSAGE_TYPE_REQUEST) {
		apt_log(APT_PRIO_WARNING,"Invalid MPF Message Type [%d]",request->message_type);
		return FALSE;
	}

	parent_task = apt_task_parent_get(engine->base);
	if(!parent_task) {
		apt_log(APT_PRIO_WARNING,"No MPF Parent Task",request->message_type);
		return FALSE;
	}

	response_msg = apt_task_msg_get(engine->base);
	response = (mpf_message_t*) response_msg->data;
	*response = *request;
	response->status_code = MPF_STATUS_CODE_SUCCESS;
	switch(request->command_id) {
		case MPF_COMMAND_ADD:
		{
			mpf_context_t *context = request->context;
			mpf_termination_t *termination = request->termination;
			if(mpf_context_termination_add(context,termination) == FALSE) {
				response->status_code = MPF_STATUS_CODE_FAILURE;
				break;
			}
			if(context->termination_count == 1) {
				apt_log(APT_PRIO_INFO,"Add Context");
				request->context->elem = apt_list_push_back(engine->contexts,context);
			}
			break;
		}
		case MPF_COMMAND_SUBTRACT:
		{
			mpf_context_t *context = request->context;
			mpf_termination_t *termination = request->termination;
			if(mpf_context_termination_subtract(context,termination) == FALSE) {
				response->status_code = MPF_STATUS_CODE_FAILURE;
				break;
			}
			if(context->termination_count == 0) {
				apt_log(APT_PRIO_INFO,"Remove Context");
				apt_list_elem_remove(engine->contexts,context->elem);
				context->elem = NULL;
			}
			break;
		}
		default:
		{
			response->status_code = MPF_STATUS_CODE_FAILURE;
		}
	}

	return apt_task_msg_signal(parent_task,response_msg);
}

static void mpf_engine_main(mpf_timer_t *timer, void *data)
{
	mpf_engine_t *engine = data;
	apt_task_msg_t *msg;
	apt_list_elem_t *elem;
	mpf_context_t *context;

	/* process request queue */
	apr_thread_mutex_lock(engine->request_queue_guard);
	msg = apt_list_pop_front(engine->request_queue);
	while(msg) {
		mpf_engine_msg_process(engine,msg);
		apt_task_msg_release(msg);
		
		msg = apt_list_pop_front(engine->request_queue);
	}
	apr_thread_mutex_unlock(engine->request_queue_guard);

	/* process contexts */
	elem = apt_list_first_elem_get(engine->contexts);
	while(elem) {
		context = apt_list_elem_object_get(elem);
		if(context) {
			mpf_context_process(context);
		}
		elem = apt_list_next_elem_get(engine->contexts,elem);
	}
}
