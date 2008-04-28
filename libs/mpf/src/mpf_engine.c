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
#include "mpf_engine.h"

struct mpf_engine_t {
	apt_composite_task_t *base;
	apr_pool_t           *pool;
	apr_thread_mutex_t   *request_queue_guard;
	apt_obj_list_t       *request_queue;
};

static void mpf_engine_start(apt_task_t *task);
static void mpf_engine_terminate(apt_task_t *task);
static apt_bool_t mpf_engine_msg_signal(apt_composite_task_t *composite_task, apt_task_msg_t *msg);
static apt_bool_t mpf_engine_msg_process(apt_composite_task_t *task, apt_task_msg_t *msg);

MPF_DECLARE(mpf_engine_t*) mpf_engine_create(
									apt_composite_task_t *master_task,
									apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_composite_task_vtable_t cvtable;
	apt_task_msg_pool_t *msg_pool;
	mpf_engine_t *engine = apr_palloc(pool,sizeof(mpf_engine_t));
	engine->pool = pool;
	engine->request_queue = NULL;

	apt_task_vtable_reset(&vtable);
	apt_composite_task_vtable_reset(&cvtable);
	vtable.start = mpf_engine_start;
	vtable.terminate = mpf_engine_terminate;
	cvtable.signal_msg = mpf_engine_msg_signal;
	cvtable.signal_msg = mpf_engine_msg_process;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(mpf_message_t),pool);

	engine->base = apt_composite_task_create(engine,&vtable,&cvtable,msg_pool,pool);
	return engine;
}

MPF_DECLARE(apt_composite_task_t*) mpf_task_get(mpf_engine_t *engine)
{
	return engine->base;
}

static APR_INLINE mpf_engine_t* mpf_engine_get(apt_task_t *task)
{
	apt_composite_task_t *composite_task = apt_task_object_get(task);
	if(!composite_task) {
		return NULL;
	}
	return apt_composite_task_object_get(composite_task);
}

static void mpf_engine_start(apt_task_t *task)
{
	mpf_engine_t *engine = mpf_engine_get(task);

	engine->request_queue = apt_list_create(engine->pool);
	apr_thread_mutex_create(&engine->request_queue_guard,APR_THREAD_MUTEX_UNNESTED,engine->pool);
}

static void mpf_engine_terminate(apt_task_t *task)
{
	mpf_engine_t *engine = mpf_engine_get(task);

	apt_list_destroy(engine->request_queue);
	apr_thread_mutex_destroy(engine->request_queue_guard);
}

static apt_bool_t mpf_engine_msg_signal(apt_composite_task_t *composite_task, apt_task_msg_t *msg)
{
	mpf_engine_t *engine = apt_composite_task_object_get(composite_task);
	
	apr_thread_mutex_lock(engine->request_queue_guard);
	apt_list_push_back(engine->request_queue,msg);
	apr_thread_mutex_unlock(engine->request_queue_guard);
	return TRUE;
}

static apt_bool_t mpf_engine_msg_process(apt_composite_task_t *task, apt_task_msg_t *msg)
{
	return TRUE;
}
