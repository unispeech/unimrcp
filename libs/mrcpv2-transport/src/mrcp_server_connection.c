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
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "apt_text_stream.h"
#include "apt_task.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define MRCP_CONNECTION_MAX_COUNT 10
#define MRCP_MESSAGE_MAX_SIZE 2048

struct mrcp_connection_t {
	apr_pool_t                *pool;

	apr_socket_t              *sock; /* accepted socket */
	apr_pollfd_t               sock_pfd;

	apt_str_t                  remote_ip;
	apr_size_t                 access_count;
	apt_list_elem_t           *it;
};

struct mrcp_connection_agent_t {
	apr_pool_t              *pool;
	apt_task_t              *task;
	mrcp_resource_factory_t *resource_factory;

	apr_pollset_t           *pollset;

	apr_sockaddr_t          *sockaddr;
	apr_socket_t            *listen_sock; /* listening socket */
	apr_pollfd_t             listen_sock_pfd;

	apr_sockaddr_t          *control_sockaddr;
	apr_socket_t            *control_sock; /* control socket */
	apr_pollfd_t             control_sock_pfd;

	apr_pool_t              *sub_pool; /* pool to allocate accepted sockets from */
	apt_obj_list_t          *connection_list;

	void                                 *obj;
	const mrcp_connection_event_vtable_t *vtable;
};

typedef enum {
	CONNECTION_TASK_MSG_MODIFY_CHANNEL,
	CONNECTION_TASK_MSG_REMOVE_CHANNEL,
	CONNECTION_TASK_MSG_SEND_MESSAGE,
	CONNECTION_TASK_MSG_TERMINATE
} connection_task_msg_data_type_e;

typedef struct connection_task_msg_data_t connection_task_msg_data_t;
struct connection_task_msg_data_t {
	connection_task_msg_data_type_e type;
	mrcp_connection_agent_t        *agent;
	mrcp_control_channel_t         *channel;
	mrcp_control_descriptor_t      *descriptor;
	mrcp_message_t                 *message;
};

static apt_bool_t mrcp_server_agent_task_run(apt_task_t *task);
static apt_bool_t mrcp_server_agent_task_terminate(apt_task_t *task);


/** Create connection agent */
APT_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_create(
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_connection_agent_t *agent;
	
	apt_log(APT_PRIO_NOTICE,"Create MRCPv2 Connection Agent [TCP] %s:%hu",listen_ip,listen_port);
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
	apr_sockaddr_info_get(&agent->control_sockaddr,"127.0.0.1",APR_INET,listen_port,0,agent->pool);
	if(!agent->control_sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = mrcp_server_agent_task_run;
	vtable.terminate = mrcp_server_agent_task_terminate;
	agent->task = apt_task_create(agent,&vtable,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	agent->sub_pool = NULL;
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

/** Set connection event handler. */
APT_DECLARE(void) mrcp_server_connection_agent_handler_set(mrcp_connection_agent_t *agent, 
									void *obj, const mrcp_connection_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Set MRCP resource factory */
APT_DECLARE(void) mrcp_server_connection_resource_factory_set(
								mrcp_connection_agent_t *agent, 
								mrcp_resource_factory_t *resource_factroy)
{
	agent->resource_factory = resource_factroy;
}

/** Get task */
APT_DECLARE(apt_task_t*) mrcp_server_connection_agent_task_get(mrcp_connection_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
APT_DECLARE(void*) mrcp_server_connection_agent_object_get(mrcp_connection_agent_t *agent)
{
	return agent->obj;
}

/** Create control channel */
APT_DECLARE(mrcp_control_channel_t*) mrcp_server_control_channel_create(mrcp_connection_agent_t *agent, void *obj, apr_pool_t *pool)
{
	mrcp_control_channel_t *channel = apr_palloc(pool,sizeof(mrcp_control_channel_t));
	channel->agent = agent;
	channel->connection = NULL;
	channel->obj = obj;
	channel->pool = pool;
	return channel;
}

static apt_bool_t mrcp_server_control_message_signal(
								connection_task_msg_data_type_e type,
								mrcp_connection_agent_t *agent,
								mrcp_control_channel_t *channel,
								mrcp_control_descriptor_t *descriptor,
								mrcp_message_t *message)
{
	apr_size_t size;
	connection_task_msg_data_t task_msg_data;
	if(!agent->control_sock) {
		return FALSE;
	}
	size = sizeof(connection_task_msg_data_t);
	task_msg_data.type = type;
	task_msg_data.agent = agent;
	task_msg_data.channel = channel;
	task_msg_data.descriptor = descriptor;
	task_msg_data.message = message;

	if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&task_msg_data,&size) != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Signal Control Message");
		return FALSE;
	}
	return TRUE;
}



/** Modify MRCPv2 control channel */
APT_DECLARE(apt_bool_t) mrcp_server_control_channel_modify(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_MODIFY_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Remove MRCPv2 control channel */
APT_DECLARE(apt_bool_t) mrcp_server_control_channel_remove(mrcp_control_channel_t *channel)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_REMOVE_CHANNEL,channel->agent,channel,NULL,NULL);
}

/** Send MRCPv2 message */
APT_DECLARE(apt_bool_t) mrcp_server_control_message_send(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,channel->agent,channel,NULL,message);
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
	status = apr_socket_create(&agent->control_sock, agent->control_sockaddr->family, SOCK_DGRAM, APR_PROTO_UDP, agent->pool);
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

static mrcp_connection_t* mrcp_server_agent_connection_add(mrcp_connection_agent_t *agent, mrcp_control_descriptor_t *descriptor)
{
	mrcp_connection_t *connection;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(mrcp_connection_t));
	connection->pool = pool;
	apt_string_copy(&connection->remote_ip,&descriptor->ip,pool);
	connection->sock = NULL;
	connection->access_count = 0;

	if(apt_list_is_empty(agent->connection_list) == TRUE) {
		apr_pool_create(&agent->sub_pool,NULL);
	}
	connection->it = apt_list_push_back(agent->connection_list,connection);
	return connection;
}

static mrcp_connection_t* mrcp_server_agent_connection_find(mrcp_connection_agent_t *agent, const apt_str_t *remote_ip)
{
	mrcp_connection_t *connection = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(agent->connection_list);
	/* walk through the list of connections */
	while(elem) {
		connection = apt_list_elem_object_get(elem);
		if(connection) {
			if(apt_string_compare(&connection->remote_ip,remote_ip) == TRUE) {
				return connection;
			}
		}
		elem = apt_list_next_elem_get(agent->connection_list,elem);
	}
	return NULL;
}

static apt_bool_t mrcp_server_agent_connection_accept(mrcp_connection_agent_t *agent)
{
	apr_socket_t *sock;
	apr_sockaddr_t *sockaddr;
	mrcp_connection_t *connection;
	apt_str_t remote_ip;

	if(!agent->sub_pool) {
		apt_log(APT_PRIO_INFO,"No Active Connection to Accept Socket for");
		return FALSE;
	}

	if(apr_socket_accept(&sock,agent->listen_sock,agent->sub_pool) != APR_SUCCESS) {
		return FALSE;
	}

	apt_string_reset(&remote_ip);
	apr_socket_addr_get(&sockaddr,APR_REMOTE,sock);
	if(apr_sockaddr_ip_get(&remote_ip.buf,sockaddr) == APR_SUCCESS) {
		remote_ip.length = strlen(remote_ip.buf);
	}
	connection = mrcp_server_agent_connection_find(agent,&remote_ip);
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

static apt_bool_t mrcp_server_agent_channel_modify(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	mrcp_connection_t *connection = NULL;
	mrcp_control_descriptor_t *answer;
	answer = apr_palloc(channel->pool,sizeof(mrcp_control_descriptor_t));
	mrcp_control_descriptor_init(answer);
	*answer = *descriptor;
	answer->setup_type = MRCP_SETUP_TYPE_PASSIVE;
	if(answer->port) {
		answer->port = agent->sockaddr->port;

		if(descriptor->connection_type == MRCP_CONNECTION_TYPE_EXISTING) {
			connection = mrcp_server_agent_connection_find(agent,&descriptor->ip);
			if(connection) {
				/* send answer */
				if(!channel->connection) {
					channel->connection = connection;
					connection->access_count ++;
				}
				if(agent->vtable && agent->vtable->on_modify) {
					agent->vtable->on_modify(channel,answer);
				}
				return TRUE;
			}
			/* no existing connection found, proceed with the new one */
			answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
		}
		
		/* create new connection */
		connection = mrcp_server_agent_connection_add(agent,descriptor);
		if(connection) {
			if(!channel->connection) {
				channel->connection = connection;
				connection->access_count ++;
			}
		}
	}
	/* send answer */
	if(agent->vtable && agent->vtable->on_modify) {
		agent->vtable->on_modify(channel,answer);
	}
	return TRUE;
}

static apt_bool_t mrcp_server_agent_channel_remove(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel)
{
	mrcp_connection_t *connection = channel->connection;
	if(connection && connection->access_count) {
		connection->access_count--;
		if(!connection->access_count) {
			/* remove from the list */
			if(connection->it) {
				apt_list_elem_remove(agent->connection_list,connection->it);
			}
			if(connection->sock) {
				apr_pollset_remove(agent->pollset,&connection->sock_pfd);
				apr_socket_close(connection->sock);
			}
			apr_pool_destroy(connection->pool);
			if(apt_list_is_empty(agent->connection_list) == TRUE) {
				apr_pool_destroy(agent->sub_pool);
				agent->sub_pool = NULL;
			}
		}
	}
	/* send response */
	if(agent->vtable && agent->vtable->on_remove) {
		agent->vtable->on_remove(channel);
	}
	return TRUE;
}

static apt_bool_t mrcp_server_agent_messsage_send(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, mrcp_message_t *message)
{
	apt_bool_t status = FALSE;
	if(connection && connection->sock) {
		char buffer[MRCP_MESSAGE_MAX_SIZE];
		apt_text_stream_t text_stream;
		
		text_stream.text.buf = buffer;
		text_stream.text.length = sizeof(buffer)-1;
		text_stream.pos = text_stream.text.buf;

		if(mrcp_message_generate(agent->resource_factory,message,&text_stream) == TRUE) {
			*text_stream.pos = '\0';
			apt_log(APT_PRIO_INFO,"Send MRCPv2 Message size=%lu\n%s",
				text_stream.text.length,text_stream.text.buf);
			if(apr_socket_send(connection->sock,text_stream.text.buf,&text_stream.text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_PRIO_WARNING,"Failed to Send MRCPv2 Message");
			}
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Generate MRCPv2 Message");
		}
	}

	return status;
}

static apt_bool_t mrcp_server_agent_messsage_receive(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	char buffer[MRCP_MESSAGE_MAX_SIZE];
	apt_bool_t more_messages_on_buffer = FALSE;
	apr_status_t status;
	apt_text_stream_t text_stream;
	mrcp_message_t *message;

	if(!connection || !connection->sock) {
		return FALSE;
	}
	
	text_stream.text.buf = buffer;
	text_stream.text.length = sizeof(buffer)-1;
	status = apr_socket_recv(connection->sock, text_stream.text.buf, &text_stream.text.length);
	if(status == APR_EOF || text_stream.text.length == 0) {
		apt_log(APT_PRIO_NOTICE,"MRCPv2 Client Disconnected");
		apr_pollset_remove(agent->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;
		if(!connection->access_count) {
			apr_pool_destroy(connection->pool);
			if(apt_list_is_empty(agent->connection_list) == TRUE) {
				apr_pool_destroy(agent->sub_pool);
				agent->sub_pool = NULL;
			}
		}
		return TRUE;
	}
	text_stream.text.buf[text_stream.text.length] = '\0';
	text_stream.pos = text_stream.text.buf;

	apt_log(APT_PRIO_INFO,"Receive MRCPv2 Message size=%lu\n%s",text_stream.text.length,text_stream.text.buf);
	if(!connection->access_count) {
		return FALSE;
	}

	do {
		message = mrcp_message_create(connection->pool);
		if(mrcp_message_parse(agent->resource_factory,message,&text_stream) == TRUE) {
			agent->vtable->on_receive(agent,connection,message);
		}
		else {
			mrcp_message_t *response;
			apt_log(APT_PRIO_WARNING,"Failed to Parse MRCPv2 Message\n");
			if(message->start_line.version == MRCP_VERSION_2) {
				/* assume that at least message length field is valid */
				if(message->start_line.length <= text_stream.text.length) {
					/* skip to the end of the message */
					text_stream.pos = text_stream.text.buf + message->start_line.length;
				}
				else {
					/* skip to the end of the buffer (support incomplete) */
					text_stream.pos = text_stream.text.buf + text_stream.text.length;
				}
			}
			response = mrcp_response_create(message,message->pool);
			response->start_line.status_code = MRCP_STATUS_CODE_UNRECOGNIZED_MESSAGE;
			if(mrcp_server_agent_messsage_send(agent,connection,response) == FALSE) {
				apt_log(APT_PRIO_WARNING,"Failed to Send MRCPv2 Response\n");
			}
		}

		more_messages_on_buffer = FALSE;
		if(text_stream.text.length > (apr_size_t)(text_stream.pos - text_stream.text.buf)) {
			/* there are more MRCPv2 messages to signal */
			more_messages_on_buffer = TRUE;
			text_stream.text.length -= text_stream.pos - text_stream.text.buf;
			text_stream.text.buf = text_stream.pos;
			apt_log(APT_PRIO_DEBUG,"Saving Remaining Buffer for Next Message");
		}
	}
	while(more_messages_on_buffer);

	return TRUE;
}

static apt_bool_t mrcp_server_agent_control_pocess(mrcp_connection_agent_t *agent)
{
	connection_task_msg_data_t task_msg_data;
	apr_size_t size = sizeof(connection_task_msg_data_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&task_msg_data, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	switch(task_msg_data.type) {
		case CONNECTION_TASK_MSG_MODIFY_CHANNEL:
		{
			mrcp_server_agent_channel_modify(agent,task_msg_data.channel,task_msg_data.descriptor);
			break;
		}
		case CONNECTION_TASK_MSG_REMOVE_CHANNEL:
		{
			mrcp_server_agent_channel_remove(agent,task_msg_data.channel);
			break;
		}
		case CONNECTION_TASK_MSG_SEND_MESSAGE:
		{
			mrcp_server_agent_messsage_send(agent,task_msg_data.channel->connection,task_msg_data.message);
			break;
		}
		case CONNECTION_TASK_MSG_TERMINATE:
			return FALSE;
	}

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
				if(mrcp_server_agent_control_pocess(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
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

		connection_task_msg_data_t data;
		apr_size_t size = sizeof(connection_task_msg_data_t);
		data.type = CONNECTION_TASK_MSG_TERMINATE;
		if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&data,&size) != APR_SUCCESS) {
			apt_log(APT_PRIO_WARNING,"Failed to Send Control Message");
		}
	}
	return TRUE;
}
