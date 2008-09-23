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

#include "apt_net_server_task.h"
#include "apt_task.h"
#include "apt_pollset.h"
#include "apt_cyclic_queue.h"
#include "apt_log.h"


/** Network server task */
struct apt_net_server_task_t {
	apr_pool_t                    *pool;
	apt_task_t                    *base;
	void                          *obj;

	apr_size_t                     max_connection_count;

	apr_thread_mutex_t            *guard;
	apt_cyclic_queue_t            *msg_queue;
	apt_pollset_t                 *pollset;

	/* Listening socket descriptor */
	apr_sockaddr_t                *sockaddr;
	apr_socket_t                  *listen_sock;
	apr_pollfd_t                   listen_sock_pfd;

	const apt_net_server_vtable_t *server_vtable;
};

static apt_bool_t apt_net_server_task_msg_signal(apt_task_t *task, apt_task_msg_t *msg);
static apt_bool_t apt_net_server_task_task_run(apt_task_t *task);

/** Create connection task */
APT_DECLARE(apt_net_server_task_t*) apt_net_server_task_create(
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										void *obj,
										apt_task_vtable_t *task_vtable,
										apt_net_server_vtable_t *server_vtable,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_net_server_task_t *task;
	
	apt_log(APT_PRIO_NOTICE,"Create Network Server Task %s:%hu [%d]",listen_ip,listen_port,max_connection_count);
	task = apr_palloc(pool,sizeof(apt_net_server_task_t));
	task->pool = pool;
	task->obj = obj;
	task->sockaddr = NULL;
	task->listen_sock = NULL;
	task->pollset = NULL;
	task->max_connection_count = max_connection_count;

	apr_sockaddr_info_get(&task->sockaddr,listen_ip,APR_INET,listen_port,0,task->pool);
	if(!task->sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = apt_net_server_task_task_run;
	vtable.signal_msg = apt_net_server_task_msg_signal;
	task->base = apt_task_create(task,&vtable,msg_pool,pool);
	if(!task->base) {
		return NULL;
	}

	task->msg_queue = apt_cyclic_queue_create(100,pool);
	apr_thread_mutex_create(&task->guard,APR_THREAD_MUTEX_UNNESTED,pool);
	return task;
}

/** Destroy connection task. */
APT_DECLARE(apt_bool_t) apt_net_server_task_destroy(apt_net_server_task_t *task)
{
	apt_log(APT_PRIO_NOTICE,"Destroy Network Server Task");
	if(task->guard) {
		apr_thread_mutex_destroy(task->guard);
		task->guard = NULL;
	}
	return apt_task_destroy(task->base);
}

/** Start connection task. */
APT_DECLARE(apt_bool_t) apt_net_server_task_start(apt_net_server_task_t *task)
{
	return apt_task_start(task->base);
}

/** Terminate connection task. */
APT_DECLARE(apt_bool_t) apt_net_server_task_terminate(apt_net_server_task_t *task)
{
	return apt_task_terminate(task->base,TRUE);
}

/** Get task */
APT_DECLARE(apt_task_t*) apt_net_server_task_base_get(apt_net_server_task_t *task)
{
	return task->base;
}

/** Get external object */
APT_DECLARE(void*) apt_net_server_task_object_get(apt_net_server_task_t *task)
{
	return task->obj;
}


/** Create listening socket and add to pollset */
static apt_bool_t apt_net_server_task_listen_socket_create(apt_net_server_task_t *task)
{
	apr_status_t status;
	if(!task->sockaddr) {
		return FALSE;
	}

	/* create listening socket */
	status = apr_socket_create(&task->listen_sock, task->sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, task->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}

	apr_socket_opt_set(task->listen_sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(task->listen_sock, -1);
	apr_socket_opt_set(task->listen_sock, APR_SO_REUSEADDR, 1);

	status = apr_socket_bind(task->listen_sock, task->sockaddr);
	if(status != APR_SUCCESS) {
		apr_socket_close(task->listen_sock);
		task->listen_sock = NULL;
		return FALSE;
	}
	status = apr_socket_listen(task->listen_sock, SOMAXCONN);
	if(status != APR_SUCCESS) {
		apr_socket_close(task->listen_sock);
		task->listen_sock = NULL;
		return FALSE;
	}

	task->listen_sock_pfd.desc_type = APR_POLL_SOCKET;
	task->listen_sock_pfd.reqevents = APR_POLLIN;
	task->listen_sock_pfd.desc.s = task->listen_sock;
	task->listen_sock_pfd.client_data = task->listen_sock;
	if(apt_pollset_add(task->pollset, &task->listen_sock_pfd) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Add Listen Socket to Pollset");
		apr_socket_close(task->listen_sock);
		task->listen_sock = NULL;
	}

	return TRUE;
}

/** Remove from pollset and destroy listening socket */
static void apt_net_server_task_listen_socket_destroy(apt_net_server_task_t *task)
{
	apt_pollset_remove(task->pollset,&task->listen_sock_pfd);

	if(task->listen_sock) {
		apr_socket_close(task->listen_sock);
		task->listen_sock = NULL;
	}
}

/** Create the pollset */
static apt_bool_t apt_net_server_task_pollset_create(apt_net_server_task_t *task)
{
	/* create pollset */
	task->pollset = apt_pollset_create((apr_uint32_t)task->max_connection_count + 2, task->pool);
	if(!task->pollset) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* create listening socket */
	if(apt_net_server_task_listen_socket_create(task) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Listen Socket");
	}

	return TRUE;
}

/** Destroy the pollset */
static void apt_net_server_task_pollset_destroy(apt_net_server_task_t *task)
{
	apt_net_server_task_listen_socket_destroy(task);
	if(task->pollset) {
		apt_pollset_destroy(task->pollset);
		task->pollset = NULL;
	}
}

static apt_bool_t apt_net_server_task_pocess(apt_net_server_task_t *task)
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

static apt_bool_t apt_net_server_task_accept(apt_net_server_task_t *task)
{
	apr_sockaddr_t  *sockaddr = NULL;
	apt_net_server_connection_t *connection;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return FALSE;
	}
	
	connection = apr_palloc(pool,sizeof(apt_net_server_connection_t));
	connection->pool = pool;
	connection->obj = NULL;
	connection->sock = NULL;

	if(apr_socket_accept(&connection->sock,task->listen_sock,connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return FALSE;
	}
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apt_pollset_add(task->pollset,&connection->sock_pfd) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Add to Pollset");
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return FALSE;
	}

	if(apr_socket_addr_get(&sockaddr,APR_REMOTE,connection->sock) == APR_SUCCESS) {
		char *client_ip = NULL;
		if(apr_sockaddr_ip_get(&client_ip,sockaddr) == APR_SUCCESS) {
			apt_log(APT_PRIO_NOTICE,"Accepted RTSP Connection %s:%hu",
				client_ip, sockaddr->port);
		}
	}
	task->server_vtable->on_connect(connection);
	return TRUE;
}

static apt_bool_t apt_net_server_task_task_run(apt_task_t *base)
{
	apt_net_server_task_t *task = apt_task_object_get(base);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!task) {
		apt_log(APT_PRIO_WARNING,"Failed to Start Network Server Task");
		return FALSE;
	}

	if(apt_net_server_task_pollset_create(task) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	while(running) {
		status = apt_pollset_poll(task->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(ret_pfd[i].desc.s == task->listen_sock) {
				apt_log(APT_PRIO_DEBUG,"Accept Connection");
				apt_net_server_task_accept(task);
				continue;
			}
			if(apt_pollset_is_wakeup(task->pollset,&ret_pfd[i])) {
				apt_log(APT_PRIO_DEBUG,"Process Control Message");
				if(apt_net_server_task_pocess(task) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_PRIO_DEBUG,"Process Message");
			task->server_vtable->on_receive(ret_pfd[i].client_data);
		}
	}

	apt_net_server_task_pollset_destroy(task);

	apt_task_child_terminate(task->base);
	return TRUE;
}

static apt_bool_t apt_net_server_task_msg_signal(apt_task_t *base, apt_task_msg_t *msg)
{
	apt_bool_t status;
	apt_net_server_task_t *task = apt_task_object_get(base);
	apr_thread_mutex_lock(task->guard);
	status = apt_cyclic_queue_push(task->msg_queue,msg);
	apr_thread_mutex_unlock(task->guard);
	if(apt_pollset_wakeup(task->pollset) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Signal Control Message");
		status = FALSE;
	}
	return status;
}


/** Close connection */
APT_DECLARE(apt_bool_t) apt_net_server_connection_close(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	if(connection->sock) {
		apt_log(APT_PRIO_NOTICE,"Close Connection");
		apt_pollset_remove(task->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
		task->server_vtable->on_disconnect(connection);
	}
	return TRUE;
}

/** Destroy connection */
APT_DECLARE(void) apt_net_server_connection_destroy(apt_net_server_connection_t *connection)
{
	if(connection->pool) {
		apr_pool_destroy(connection->pool);
	}
}
