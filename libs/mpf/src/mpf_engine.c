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

#include "apt_obj_list.h"
#include "mpf_engine.h"
#include "mpf_timer.h"

struct mpf_engine_t {
	apt_task_t         *base;
	apr_pool_t         *pool;
	apr_thread_mutex_t *request_queue_guard;
	apt_obj_list_t     *request_queue;
	mpf_timer_t        *timer;
};

static void mpf_engine_main(mpf_timer_t *timer, void *data);
static apt_bool_t mpf_engine_start(apt_task_t *task);
static apt_bool_t mpf_engine_terminate(apt_task_t *task);
static apt_bool_t mpf_engine_msg_signal(apt_task_t *task, apt_task_msg_t *msg);

MPF_DECLARE(mpf_engine_t*) mpf_engine_create(apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	mpf_engine_t *engine = apr_palloc(pool,sizeof(mpf_engine_t));
	engine->pool = pool;
	engine->request_queue = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.start = mpf_engine_start;
	vtable.terminate = mpf_engine_terminate;
	vtable.signal_msg = mpf_engine_msg_signal;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),pool);

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

	engine->timer = mpf_timer_start(10,mpf_engine_main,engine,engine->pool);
	return TRUE;
}

static apt_bool_t mpf_engine_terminate(apt_task_t *task)
{
	mpf_engine_t *engine = apt_task_object_get(task);

	mpf_timer_stop(engine->timer);

	apt_list_destroy(engine->request_queue);
	apr_thread_mutex_destroy(engine->request_queue_guard);
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

static apt_bool_t mpf_engine_msg_process(mpf_engine_t *engine, apt_task_msg_t *msg)
{
	mpf_message_t *mpf_message = (mpf_message_t*) msg->data;
	if(mpf_message->message_type != MPF_MESSAGE_TYPE_REQUEST) {
		return FALSE;
	}
	
	switch(mpf_message->action_type) {
		case MPF_ACTION_TYPE_CONTEXT:
		{
			break;
		}
		case MPF_ACTION_TYPE_TERMINATION:
		{
			break;
		}
	}
	return TRUE;
}

static void mpf_engine_main(mpf_timer_t *timer, void *data)
{
	mpf_engine_t *engine = data;
	apt_task_msg_t *msg;
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
}
