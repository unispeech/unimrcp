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
#include "mrcp_server_connection.h"
#include "mrcp_control_descriptor.h"
#include "apt_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define MRCP_CONNECTION_MAX_COUNT 10

struct mrcp_connection_t {
	apr_pool_t                *pool;

	apr_socket_t              *sock; /* accepted socket */
	apr_pollfd_t               sock_pfd;

	mrcp_control_descriptor_t *descriptor;
	size_t                     access_count;
};

struct mrcp_connection_agent_t {
	apr_pool_t     *pool;
	apt_task_t     *task;

	apr_pollset_t  *pollset;

	apr_sockaddr_t *sockaddr;
	apr_socket_t   *listen_sock; /* listening socket */
	apr_pollfd_t    listen_sock_pfd;

	apr_sockaddr_t *control_sockaddr;
	apr_socket_t   *control_sock; /* control socket */
	apr_pollfd_t    control_sock_pfd;

	apt_obj_list_t *connection_list;
};

typedef enum {
	CONNECTION_AGENT_MESSAGE_OFFER,
	CONNECTION_AGENT_MESSAGE_TERMINATE
}connection_agent_message_type_e ;

typedef struct connection_agent_message_t connection_agent_message_t;
struct connection_agent_message_t {
	connection_agent_message_type_e type;
	mrcp_connection_agent_t        *agent;
	void                           *handle;
	mrcp_connection_t              *connection;
	mrcp_control_descriptor_t      *descriptor;
};

static apt_bool_t mrcp_server_agent_task_run(apt_task_t *task);
static apt_bool_t mrcp_server_agent_task_terminate(apt_task_t *task);


/** Create connection agent */
APT_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_create(
										void *obj,
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_connection_agent_t *agent;
	
	apt_log(APT_PRIO_NOTICE,"Create MRCPv2 Agent TCP %s:%hu",listen_ip,listen_port);
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->sockaddr = NULL;
	agent->listen_sock = NULL;
	agent->control_sockaddr = NULL;
	agent->control_sock = NULL;
	agent->pollset = NULL;

	apr_sockaddr_info_get(&agent->sockaddr,listen_ip,APR_INET,listen_port,0,agent->pool);
	if(!agent->sockaddr) {
		return NULL;
	}
	apr_sockaddr_info_get(&agent->control_sockaddr,"127.0.0.1",APR_INET,0,0,agent->pool);
	if(!agent->sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = mrcp_server_agent_task_run;
	vtable.terminate = mrcp_server_agent_task_terminate;
	agent->task = apt_task_create(agent,&vtable,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	agent->connection_list = apt_list_create(pool);
	return agent;
}

/** Destroy connection agent. */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_PRIO_NOTICE,"Destroy MRCPv2 Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}


/** Offer MRCPv2 connection descriptor */
APT_DECLARE(apt_bool_t) mrcp_server_connection_agent_offer(mrcp_connection_agent_t *agent,
												  void *handle,
												  mrcp_connection_t *connection,
												  mrcp_control_descriptor_t *descriptor)
{
	if(agent->control_sock) {
		connection_agent_message_t message;
		apr_size_t size = sizeof(connection_agent_message_t);
		message.type = CONNECTION_AGENT_MESSAGE_OFFER;
		message.agent = agent;
		message.handle = handle;
		message.connection = connection;
		message.descriptor = descriptor;
		apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&message,&size);
	}
	return TRUE;
}

/** Get task */
APT_DECLARE(apt_task_t*) mrcp_server_connection_agent_task_get(mrcp_connection_agent_t *agent)
{
	return agent->task;
}


static apt_bool_t mrcp_server_agent_socket_create(mrcp_connection_agent_t *agent)
{
	apr_status_t status;

	if(!agent->sockaddr || !agent->control_sockaddr) {
		return FALSE;
	}

	/* create listening socket */
	status = apr_socket_create(&agent->listen_sock, agent->sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, agent->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}

	apr_socket_opt_set(agent->listen_sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(agent->listen_sock, -1);
	apr_socket_opt_set(agent->listen_sock, APR_SO_REUSEADDR, 1);

	status = apr_socket_bind(agent->listen_sock, agent->sockaddr);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}
	status = apr_socket_listen(agent->listen_sock, SOMAXCONN);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}

	/* create control socket */
	status = apr_socket_create(&agent->control_sock, agent->control_sockaddr->family, SOCK_DGRAM, 0, agent->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}
	status = apr_socket_bind(agent->control_sock, agent->control_sockaddr);
	if(status != APR_SUCCESS) {
		return FALSE;
	}


	/* create pollset */
	status = apr_pollset_create(&agent->pollset, MRCP_CONNECTION_MAX_COUNT + 2, agent->pool, 0);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		return FALSE;
	}
	
	agent->listen_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->listen_sock_pfd.reqevents = APR_POLLIN;
	agent->listen_sock_pfd.desc.s = agent->listen_sock;
	agent->listen_sock_pfd.client_data = agent->listen_sock;
	status = apr_pollset_add(agent->pollset, &agent->listen_sock_pfd);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
		return FALSE;
	}

	agent->control_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->control_sock_pfd.reqevents = APR_POLLIN;
	agent->control_sock_pfd.desc.s = agent->control_sock;
	agent->control_sock_pfd.client_data = agent->control_sock;
	status = apr_pollset_add(agent->pollset, &agent->control_sock_pfd);
	if(status != APR_SUCCESS) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
		return FALSE;
	}

	return TRUE;
}

static mrcp_connection_t* mrcp_server_agent_connection_find(mrcp_connection_agent_t *agent, const char *remote_ip)
{
	mrcp_connection_t *connection = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(agent->connection_list);
	/* walk through the list of connections */
	while(elem) {
		connection = apt_list_elem_object_get(elem);
		if(connection && connection->descriptor) {
/*			if(strcmp(connection->descriptor->,remote_ip) == 0) {
				return connection;
*/		}
		elem = apt_list_next_elem_get(agent->connection_list,elem);
	}
	return NULL;
}

static apt_bool_t mrcp_server_agent_connection_accept(mrcp_connection_agent_t *agent)
{
	apr_socket_t *sock;
	apr_sockaddr_t *sockaddr;
	mrcp_connection_t *connection;
	char *remote_ip;

	apr_status_t status = apr_socket_accept(&sock, agent->listen_sock, agent->pool);
	if(status != APR_SUCCESS) {
		return FALSE;
	}

	apr_socket_addr_get(&sockaddr,APR_REMOTE,sock);
	apr_sockaddr_ip_get(&remote_ip,sockaddr);
	connection = mrcp_server_agent_connection_find(agent,remote_ip);

	if(connection && !connection->sock) {
		apt_log(APT_PRIO_NOTICE,"MRCPv2 Client Connected");
		connection->sock = sock;
		connection->sock_pfd.desc_type = APR_POLL_SOCKET;
		connection->sock_pfd.reqevents = APR_POLLIN;
		connection->sock_pfd.desc.s = connection->sock;
		connection->sock_pfd.client_data = connection;
		apr_pollset_add(agent->pollset, &connection->sock_pfd);
	}
	else {
		apt_log(APT_PRIO_INFO,"MRCPv2 Client Rejected");
		apr_socket_close(sock);
	}

	return TRUE;
}

static apt_bool_t mrcp_server_agent_control_pocess(mrcp_connection_agent_t *agent)
{
	connection_agent_message_t message;
	apr_size_t size = sizeof(connection_agent_message_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&message, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	if(message.type == CONNECTION_AGENT_MESSAGE_TERMINATE) {
		return FALSE;
	}

	return TRUE;
}

static apt_bool_t mrcp_server_agent_messsage_receive(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	return TRUE;
}

static apt_bool_t mrcp_server_agent_task_run(apt_task_t *task)
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

	if(mrcp_server_agent_socket_create(agent) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create MRCPv2 Agent Socket");
		return FALSE;
	}

	while(running) {
		status = apr_pollset_poll(agent->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(ret_pfd[i].desc.s == agent->listen_sock) {
				apt_log(APT_PRIO_DEBUG,"Accept MRCPv2 Connection");
				mrcp_server_agent_connection_accept(agent);
				continue;
			}
			if(ret_pfd[i].desc.s == agent->control_sock) {
				apt_log(APT_PRIO_DEBUG,"Process Control Message");
				mrcp_server_agent_control_pocess(agent);
				running = FALSE;break;//continue;
			}
	
			apt_log(APT_PRIO_DEBUG,"Process MRCPv2 Message");
			mrcp_server_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t mrcp_server_agent_task_terminate(apt_task_t *task)
{
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	if(agent->control_sock) {
		connection_agent_message_t message;
		apr_size_t size = sizeof(connection_agent_message_t);
		message.type = CONNECTION_AGENT_MESSAGE_TERMINATE;
		apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&message,&size);
	}
	return TRUE;
}
