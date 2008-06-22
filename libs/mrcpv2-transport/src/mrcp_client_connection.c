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

#include <apr_poll.h>
#include "mrcp_client_connection.h"
#include "mrcp_control_descriptor.h"
#include "apt_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define MRCP_CONNECTION_MAX_COUNT 10

struct mrcp_connection_t {
	apr_pool_t                *pool;

	apr_sockaddr_t            *sockaddr;
	apr_socket_t              *sock; /* connected socket */
	apr_pollfd_t               sock_pfd;

	apr_size_t                 access_count;
	apt_list_elem_t           *it;
};

struct mrcp_connection_agent_t {
	apr_pool_t     *pool;
	apt_task_t     *task;

	apr_pollset_t  *pollset;

	apr_sockaddr_t *control_sockaddr;
	apr_socket_t   *control_sock; /* control socket */
	apr_pollfd_t    control_sock_pfd;

	apt_obj_list_t *connection_list;

	void                                 *obj;
	const mrcp_connection_event_vtable_t *vtable;
};

typedef enum {
	CONNECTION_AGENT_MESSAGE_MODIFY_CONNECTION,
	CONNECTION_AGENT_MESSAGE_REMOVE_CONNECTION,
	CONNECTION_AGENT_MESSAGE_TERMINATE
}connection_agent_message_type_e ;

typedef struct connection_agent_message_t connection_agent_message_t;
struct connection_agent_message_t {
	connection_agent_message_type_e type;
	mrcp_connection_agent_t        *agent;
	void                           *handle;
	mrcp_connection_t              *connection;
	mrcp_control_descriptor_t      *descriptor;
	apr_pool_t                     *pool;
};


static apt_bool_t mrcp_client_agent_task_run(apt_task_t *task);
static apt_bool_t mrcp_client_agent_task_terminate(apt_task_t *task);

/** Create connection agent. */
APT_DECLARE(mrcp_connection_agent_t*) mrcpv2_client_connection_agent_create(apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_connection_agent_t *agent;
	
	apt_log(APT_PRIO_NOTICE,"Create MRCPv2 Connection Agent [TCP]");
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->control_sockaddr = NULL;
	agent->control_sock = NULL;
	agent->pollset = NULL;

	apr_sockaddr_info_get(&agent->control_sockaddr,"127.0.0.1",APR_INET,7856,0,agent->pool);
	if(!agent->control_sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = mrcp_client_agent_task_run;
	vtable.terminate = mrcp_client_agent_task_terminate;
	agent->task = apt_task_create(agent,&vtable,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	agent->connection_list = apt_list_create(pool);
	return agent;
}

/** Destroy connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_client_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_PRIO_NOTICE,"Destroy MRCPv2 Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_client_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_client_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}

/** Set connection event handler. */
APT_DECLARE(void) mrcp_client_connection_agent_handler_set(mrcp_connection_agent_t *agent, 
									void *obj, const mrcp_connection_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}


/** Modify MRCPv2 connection descriptor */
APT_DECLARE(apt_bool_t) mrcp_client_connection_modify(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								mrcp_control_descriptor_t *descriptor,
								apr_pool_t *pool)
{
	if(agent->control_sock) {
		connection_agent_message_t message;
		apr_size_t size = sizeof(connection_agent_message_t);
		message.type = CONNECTION_AGENT_MESSAGE_MODIFY_CONNECTION;
		message.agent = agent;
		message.handle = handle;
		message.connection = connection;
		message.descriptor = descriptor;
		message.pool = pool;
		apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&message,&size);
	}
	return TRUE;
}

/** Remove MRCPv2 connection */
APT_DECLARE(apt_bool_t) mrcp_client_connection_remove(
								mrcp_connection_agent_t *agent,
								void *handle,
								mrcp_connection_t *connection,
								apr_pool_t *pool)
{
	if(agent->control_sock) {
		connection_agent_message_t message;
		apr_size_t size = sizeof(connection_agent_message_t);
		message.type = CONNECTION_AGENT_MESSAGE_REMOVE_CONNECTION;
		message.agent = agent;
		message.handle = handle;
		message.connection = connection;
		message.descriptor = NULL;
		message.pool = pool;
		apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&message,&size);
	}
	return TRUE;
}


/** Get task */
APT_DECLARE(apt_task_t*) mrcp_client_connection_agent_task_get(mrcp_connection_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
APT_DECLARE(void*) mrcp_client_connection_agent_object_get(mrcp_connection_agent_t *agent)
{
	return agent->obj;
}



static apt_bool_t mrcp_client_agent_socket_create(mrcp_connection_agent_t *agent)
{
	apr_status_t status;

	if(!agent->control_sockaddr) {
		return FALSE;
	}

	/* create control socket */
	status = apr_socket_create(&agent->control_sock, agent->control_sockaddr->family, SOCK_DGRAM, APR_PROTO_UDP, agent->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}
	status = apr_socket_bind(agent->control_sock, agent->control_sockaddr);
	if(status != APR_SUCCESS) {
		return FALSE;
	}

	/* create pollset */
	status = apr_pollset_create(&agent->pollset, MRCP_CONNECTION_MAX_COUNT + 1, agent->pool, 0);
	if(status != APR_SUCCESS) {
		return FALSE;
	}
	
	agent->control_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->control_sock_pfd.reqevents = APR_POLLIN;
	agent->control_sock_pfd.desc.s = agent->control_sock;
	agent->control_sock_pfd.client_data = agent->control_sock;
	status = apr_pollset_add(agent->pollset, &agent->control_sock_pfd);
	if(status != APR_SUCCESS) {
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
		return FALSE;
	}

	return TRUE;
}

static mrcp_connection_t* mrcp_client_agent_connection_create(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	mrcp_connection_t *connection;
	apr_sockaddr_t *sockaddr = NULL;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(mrcp_connection_t));
	connection->pool = pool;
	connection->sockaddr = sockaddr;
	connection->sock = NULL;

	apr_sockaddr_info_get(&sockaddr,descriptor->ip.buf,APR_INET,descriptor->port,0,connection->pool);
	if(!sockaddr) {
		apr_pool_destroy(pool);
		return NULL;
	}

	if(apr_socket_create(&connection->sock, sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, connection->pool) != APR_SUCCESS) {
		apr_pool_destroy(pool);
		return NULL;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock, sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}

	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apr_pollset_add(agent->pollset, &connection->sock_pfd) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		apr_pool_destroy(pool);
		return NULL;
	}
	
	apt_log(APT_PRIO_NOTICE,"Connected to MRCPv2 Server\n");
	connection->access_count = 1;
	connection->it = apt_list_push_back(agent->connection_list,connection);
	return connection;
}

static mrcp_connection_t* mrcp_client_agent_connection_find(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	apr_sockaddr_t *sockaddr;
	mrcp_connection_t *connection = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(agent->connection_list);
	/* walk through the list of connections */
	while(elem) {
		connection = apt_list_elem_object_get(elem);
		if(connection) {
			if(apr_sockaddr_info_get(&sockaddr,descriptor->ip.buf,APR_INET,descriptor->port,0,connection->pool) == APR_SUCCESS) {
				if(apr_sockaddr_equal(sockaddr,connection->sockaddr) != 0) {
					return connection;
				}
			}
		}
		elem = apt_list_next_elem_get(agent->connection_list,elem);
	}
	return NULL;
}

static apt_bool_t mrcp_client_agent_control_pocess(mrcp_connection_agent_t *agent)
{
	connection_agent_message_t message;
	apr_size_t size = sizeof(connection_agent_message_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&message, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	switch(message.type) {
		case CONNECTION_AGENT_MESSAGE_MODIFY_CONNECTION:
		{
			mrcp_connection_t *connection = NULL;
			mrcp_control_descriptor_t *answer = message.descriptor;
			if(answer->port) {
				if(answer->connection_type == MRCP_CONNECTION_TYPE_EXISTING) {
					connection = mrcp_client_agent_connection_find(agent,answer);
					if(connection) {
						connection->access_count ++;
						/* send response */
						if(agent->vtable && agent->vtable->on_modify) {
							agent->vtable->on_modify(agent,message.handle,message.connection,answer);
						}
						break;
					}
					/* no existing connection found, proceed with the new one */
				}
				/* create new connection */
				connection = mrcp_client_agent_connection_create(agent,answer);
			}
			/* send response */
			if(agent->vtable && agent->vtable->on_modify) {
				agent->vtable->on_modify(agent,message.handle,connection,answer);
			}
			break;
		}
		case CONNECTION_AGENT_MESSAGE_REMOVE_CONNECTION:
		{
			mrcp_connection_t *connection = message.connection;
			if(connection && connection->access_count) {
				connection->access_count--;
				if(!connection->access_count) {
					/* remove from the list */
					if(connection->it) {
						apt_list_elem_remove(agent->connection_list,connection->it);
					}
					if(!connection->sock) {
						apr_pool_destroy(connection->pool);
					}
				}
			}
			/* send response */
			if(agent->vtable && agent->vtable->on_remove) {
				agent->vtable->on_remove(agent,message.handle);
			}
			break;
		}
		case CONNECTION_AGENT_MESSAGE_TERMINATE:
			return FALSE;
	}

	return TRUE;
}

static apt_bool_t mrcp_client_agent_messsage_receive(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	return TRUE;
}

static apt_bool_t mrcp_client_agent_task_run(apt_task_t *task)
{
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!agent) {
		apt_log(APT_PRIO_WARNING,"Failed to Start MRCPv2 Agent");
		return FALSE;
	}

	if(mrcp_client_agent_socket_create(agent) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create MRCPv2 Agent Socket");
		return FALSE;
	}

	while(running) {
		status = apr_pollset_poll(agent->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(ret_pfd[i].desc.s == agent->control_sock) {
				apt_log(APT_PRIO_DEBUG,"Process Control Message");
				if(mrcp_client_agent_control_pocess(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_PRIO_DEBUG,"Process MRCPv2 Message");
			mrcp_client_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t mrcp_client_agent_task_terminate(apt_task_t *task)
{
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	if(agent->control_sock) {
		connection_agent_message_t message;
		apr_size_t size = sizeof(connection_agent_message_t);
		message.type = CONNECTION_AGENT_MESSAGE_TERMINATE;
		if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&message,&size) != APR_SUCCESS) {
			apt_log(APT_PRIO_WARNING,"Failed to Send Control Message");
		}
	}
	return TRUE;
}
