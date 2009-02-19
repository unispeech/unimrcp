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

#include "apt_net_client_task.h"
#include "apt_task.h"
#include "apt_pollset.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"


/** Network client task */
struct apt_net_client_task_t {
	apr_pool_t                    *pool;
	apt_task_t                    *base;
	void                          *obj;

	apr_size_t                     max_connection_count;

	apr_thread_mutex_t            *guard;
	apt_cyclic_queue_t            *msg_queue;
	apt_pollset_t                 *pollset;

	const apt_net_client_vtable_t *client_vtable;
};

static apt_bool_t apt_net_client_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t apt_net_client_task_run(apt_task_t *task);

/** Create connection task */
APT_DECLARE(apt_net_client_task_t*) apt_net_client_task_create(
										apr_size_t max_connection_count,
										void *obj,
										apt_task_vtable_t *task_vtable,
										const apt_net_client_vtable_t *client_vtable,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_net_client_task_t *task;
	
	task = apr_palloc(pool,sizeof(apt_net_client_task_t));
	task->pool = pool;
	task->obj = obj;
	task->pollset = NULL;
	task->max_connection_count = max_connection_count;

	if(!client_vtable || !client_vtable->on_receive) {
		return NULL;
	}
	task->client_vtable = client_vtable;

	apt_task_vtable_reset(&vtable);
	if(task_vtable) {
		vtable = *task_vtable;
	}
	vtable.run = apt_net_client_task_run;
	vtable.signal_msg = apt_net_client_task_msg_signal;
	task->base = apt_task_create(task,&vtable,msg_pool,pool);
	if(!task->base) {
		return NULL;
	}

	task->msg_queue = apt_cyclic_queue_create(100,pool);
	apr_thread_mutex_create(&task->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return task;
}

/** Destroy connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_destroy(apt_net_client_task_t *task)
{
	if(task->guard) {
		apr_thread_mutex_destroy(task->guard);
		task->guard = NULL;
	}
	return apt_task_destroy(task->base);
}

/** Start connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_start(apt_net_client_task_t *task)
{
	return apt_task_start(task->base);
}

/** Terminate connection task. */
APT_DECLARE(apt_bool_t) apt_net_client_task_terminate(apt_net_client_task_t *task)
{
	return apt_task_terminate(task->base,TRUE);
}

/** Get task */
APT_DECLARE(apt_task_t*) apt_net_client_task_base_get(apt_net_client_task_t *task)
{
	return task->base;
}

/** Get external object */
APT_DECLARE(void*) apt_net_client_task_object_get(apt_net_client_task_t *task)
{
	return task->obj;
}

/** Create connection */
APT_DECLARE(apt_net_client_connection_t*) apt_net_client_connect(apt_net_client_task_t *task, const char *ip, apr_port_t port)
{
	apr_sockaddr_t *sockaddr;
	apt_net_client_connection_t *connection;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(apt_net_client_connection_t));
	connection->pool = pool;
	connection->obj = NULL;
	connection->sock = NULL;

	if(apr_sockaddr_info_get(&sockaddr,ip,APR_INET,port,0,connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return NULL;
	}

	if(apr_socket_create(&connection->sock,sockaddr->family,SOCK_STREAM,APR_PROTO_TCP,connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return NULL;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock,sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}

	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_pollset_add(task->pollset,&connection->sock_pfd) != TRUE) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Established TCP Connection %s:%d",ip,port);
	return connection;
}

/** Close connection */
APT_DECLARE(apt_bool_t) apt_net_client_connection_close(apt_net_client_task_t *task, apt_net_client_connection_t *connection)
{
	if(connection->sock) {
		apt_pollset_remove(task->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	return TRUE;
}

/** Close and destroy connection */
APT_DECLARE(apt_bool_t) apt_net_client_disconnect(apt_net_client_task_t *task, apt_net_client_connection_t *connection)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Disconnect TCP Connection");
	apt_net_client_connection_close(task,connection);
	apr_pool_destroy(connection->pool);
	return TRUE;
}

/** Create the pollset */
static apt_bool_t apt_net_client_task_pollset_create(apt_net_client_task_t *task)
{
	/* create pollset */
	task->pollset = apt_pollset_create((apr_uint32_t)task->max_connection_count, task->pool);
	if(!task->pollset) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	return TRUE;
}

/** Destroy the pollset */
static void apt_net_client_task_pollset_destroy(apt_net_client_task_t *task)
{
	if(task->pollset) {
		apt_pollset_destroy(task->pollset);
		task->pollset = NULL;
	}
}

static apt_bool_t apt_net_client_task_process(apt_net_client_task_t *task)
{
	apt_bool_t status = TRUE;
	apt_task_msg_t *msg;

	apr_thread_mutex_lock(task->guard);
	do {
		msg = apt_cyclic_queue_pop(task->msg_queue);
		if(msg) {
			status = apt_task_msg_process(task->base,msg);
		}
	}
	while(msg);
	apr_thread_mutex_unlock(task->guard);
	return status;
}

static apt_bool_t apt_net_client_task_run(apt_task_t *base)
{
	apt_net_client_task_t *task = apt_task_object_get(base);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!task) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start Network Client Task");
		return FALSE;
	}

	if(apt_net_client_task_pollset_create(task) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	while(running) {
		status = apt_pollset_poll(task->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(apt_pollset_is_wakeup(task->pollset,&ret_pfd[i])) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Control Message");
				if(apt_net_client_task_process(task) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Message");
			task->client_vtable->on_receive(task,ret_pfd[i].client_data);
		}
	}

	apt_net_client_task_pollset_destroy(task);

	apt_task_child_terminate(task->base);
	return TRUE;
}

static apt_bool_t apt_net_client_task_msg_signal(apt_task_t *base, apt_task_msg_t *msg)
{
	apt_bool_t status;
	apt_net_client_task_t *task = apt_task_object_get(base);
	apr_thread_mutex_lock(task->guard);
	status = apt_cyclic_queue_push(task->msg_queue,msg);
	apr_thread_mutex_unlock(task->guard);
	if(apt_pollset_wakeup(task->pollset) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		status = FALSE;
	}
	return status;
}
