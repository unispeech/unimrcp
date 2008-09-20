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
#include "rtsp_server_connection.h"
#include "apt_text_stream.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define RTSP_MESSAGE_MAX_SIZE 2048

struct rtsp_server_agent_t {
	apr_pool_t              *pool;
	apt_task_t              *task;

	apr_size_t               max_connection_count;
	apr_pollset_t           *pollset;

	apr_sockaddr_t          *sockaddr;
	/* Listening socket */
	apr_socket_t            *listen_sock;
	apr_pollfd_t             listen_sock_pfd;

	apr_sockaddr_t          *control_sockaddr;
	/* Control socket */
	apr_socket_t            *control_sock;
	apr_pollfd_t             control_sock_pfd;

	apt_obj_list_t          *connection_list;

	void                                   *obj;
	const rtsp_server_agent_event_vtable_t *vtable;
};

/** RTSP connection */
struct rtsp_server_connection_t {
	/** Memory pool */
	apr_pool_t      *pool;

	/** Accepted socket */
	apr_socket_t    *sock;
	/** Socket poll descriptor */
	apr_pollfd_t     sock_pfd;
	/** Remote sockaddr */
	apr_sockaddr_t  *sockaddr;
	/** Remote IP */
	apt_str_t        remote_ip;

	/** Reference count */
	apr_size_t       access_count;
	/** Agent list element */
	apt_list_elem_t *it;
};


typedef enum {
	CONNECTION_TASK_MSG_ADD_REFERENCE,
	CONNECTION_TASK_MSG_REMOVE_REFERENCE,
	CONNECTION_TASK_MSG_SEND_MESSAGE,
	CONNECTION_TASK_MSG_TERMINATE
} connection_task_msg_data_type_e;

typedef struct connection_task_msg_data_t connection_task_msg_data_t;
struct connection_task_msg_data_t {
	connection_task_msg_data_type_e type;
	rtsp_server_agent_t            *agent;
	rtsp_server_connection_t       *connection;
	rtsp_message_t                 *message;
};

static apt_bool_t rtsp_server_agent_task_run(apt_task_t *task);
static apt_bool_t rtsp_server_agent_task_terminate(apt_task_t *task);

/** Create connection agent */
RTSP_DECLARE(rtsp_server_agent_t*) rtsp_server_connection_agent_create(
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	rtsp_server_agent_t *agent;
	
	apt_log(APT_PRIO_NOTICE,"Create RTSP Connection Agent %s:%hu [%d]",listen_ip,listen_port,max_connection_count);
	agent = apr_palloc(pool,sizeof(rtsp_server_agent_t));
	agent->pool = pool;
	agent->sockaddr = NULL;
	agent->listen_sock = NULL;
	agent->control_sockaddr = NULL;
	agent->control_sock = NULL;
	agent->pollset = NULL;
	agent->max_connection_count = max_connection_count;

	apr_sockaddr_info_get(&agent->sockaddr,listen_ip,APR_INET,listen_port,0,agent->pool);
	if(!agent->sockaddr) {
		return NULL;
	}
	apr_sockaddr_info_get(&agent->control_sockaddr,"127.0.0.1",APR_INET,listen_port,0,agent->pool);
	if(!agent->control_sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = rtsp_server_agent_task_run;
	vtable.terminate = rtsp_server_agent_task_terminate;
	agent->task = apt_task_create(agent,&vtable,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	agent->connection_list = NULL;
	return agent;
}

/** Destroy connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_destroy(rtsp_server_agent_t *agent)
{
	apt_log(APT_PRIO_NOTICE,"Destroy RTSP Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_start(rtsp_server_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_agent_terminate(rtsp_server_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}

/** Set connection event handler. */
RTSP_DECLARE(void) rtsp_server_connection_agent_handler_set(
									rtsp_server_agent_t *agent, 
									void *obj, 
									const rtsp_server_agent_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_server_connection_agent_task_get(rtsp_server_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_server_connection_agent_object_get(rtsp_server_agent_t *agent)
{
	return agent->obj;
}


static apt_bool_t rtsp_server_control_message_signal(
								connection_task_msg_data_type_e type,
								rtsp_server_agent_t *agent,
								rtsp_server_connection_t *connection,
								rtsp_message_t *message)
{
	apr_size_t size;
	connection_task_msg_data_t task_msg_data;
	if(!agent->control_sock) {
		return FALSE;
	}
	size = sizeof(connection_task_msg_data_t);
	task_msg_data.type = type;
	task_msg_data.agent = agent;
	task_msg_data.connection = connection;
	task_msg_data.message = message;

	if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&task_msg_data,&size) != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Signal Control Message");
		return FALSE;
	}
	return TRUE;
}

/** Add Reference to RTSP connection */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_reference_add(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	return rtsp_server_control_message_signal(CONNECTION_TASK_MSG_ADD_REFERENCE,agent,connection,NULL);
}

/** Remove Reference from RTSP connection */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_reference_remove(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	return rtsp_server_control_message_signal(CONNECTION_TASK_MSG_REMOVE_REFERENCE,agent,connection,NULL);
}

/** Send RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_server_connection_message_send(rtsp_server_connection_t *connection, rtsp_message_t *message)
{
	return rtsp_server_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,NULL,connection,message);
}


static apt_bool_t rtsp_server_agent_listen_socket_create(rtsp_server_agent_t *agent)
{
	apr_status_t status;
	if(!agent->sockaddr) {
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

	return TRUE;
}

static void rtsp_server_agent_listen_socket_destroy(rtsp_server_agent_t *agent)
{
	if(agent->listen_sock) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
	}
}

static apt_bool_t rtsp_server_agent_control_socket_create(rtsp_server_agent_t *agent)
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
		apr_socket_close(agent->control_sock);
		agent->control_sock = NULL;
		return FALSE;
	}

	return TRUE;
}

static void rtsp_server_agent_control_socket_destroy(rtsp_server_agent_t *agent)
{
	if(agent->control_sock) {
		apr_socket_close(agent->control_sock);
		agent->control_sock = NULL;
	}
}

static apt_bool_t rtsp_server_agent_pollset_create(rtsp_server_agent_t *agent)
{
	apr_status_t status;
	/* create pollset */
	status = apr_pollset_create(&agent->pollset, (apr_uint32_t)agent->max_connection_count + 2, agent->pool, 0);
	if(status != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* create control socket */
	if(rtsp_server_agent_control_socket_create(agent) != TRUE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Control Socket");
		apr_pollset_destroy(agent->pollset);
		return FALSE;
	}
	/* add control socket to pollset */
	agent->control_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->control_sock_pfd.reqevents = APR_POLLIN;
	agent->control_sock_pfd.desc.s = agent->control_sock;
	agent->control_sock_pfd.client_data = agent->control_sock;
	status = apr_pollset_add(agent->pollset, &agent->control_sock_pfd);
	if(status != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Add Control Socket to Pollset");
		rtsp_server_agent_control_socket_destroy(agent);
		apr_pollset_destroy(agent->pollset);
		return FALSE;
	}

	if(rtsp_server_agent_listen_socket_create(agent) == TRUE) {
		/* add listen socket to pollset */
		agent->listen_sock_pfd.desc_type = APR_POLL_SOCKET;
		agent->listen_sock_pfd.reqevents = APR_POLLIN;
		agent->listen_sock_pfd.desc.s = agent->listen_sock;
		agent->listen_sock_pfd.client_data = agent->listen_sock;
		status = apr_pollset_add(agent->pollset, &agent->listen_sock_pfd);
		if(status != APR_SUCCESS) {
			apt_log(APT_PRIO_WARNING,"Failed to Add Listen Socket to Pollset");
			rtsp_server_agent_listen_socket_destroy(agent);
		}
	}
	else {
		apt_log(APT_PRIO_WARNING,"Failed to Create Listen Socket");
	}

	return TRUE;
}

static void rtsp_server_agent_pollset_destroy(rtsp_server_agent_t *agent)
{
	rtsp_server_agent_listen_socket_destroy(agent);
	rtsp_server_agent_control_socket_destroy(agent);
	if(agent->pollset) {
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
	}
}

static apt_bool_t rtsp_connection_remove(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	if(connection->it) {
		apt_list_elem_remove(agent->connection_list,connection->it);
		connection->it = NULL;
	}
	return TRUE;
}

static rtsp_server_connection_t* rtsp_connection_create()
{
	rtsp_server_connection_t *connection;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(rtsp_server_connection_t));
	connection->pool = pool;
	apt_string_reset(&connection->remote_ip);
	connection->sockaddr = NULL;
	connection->sock = NULL;
	connection->access_count = 0;
	connection->it = NULL;
	return connection;
}

void rtsp_connection_destroy(rtsp_server_connection_t *connection)
{
	if(connection && connection->pool) {
		apr_pool_destroy(connection->pool);
	}
}

static apt_bool_t rtsp_server_agent_connection_accept(rtsp_server_agent_t *agent)
{
	rtsp_server_connection_t *connection;

	connection = rtsp_connection_create();
	if(apr_socket_accept(&connection->sock,agent->listen_sock,connection->pool) != APR_SUCCESS) {
		rtsp_connection_destroy(connection);
		return FALSE;
	}
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apr_pollset_add(agent->pollset, &connection->sock_pfd) != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Add to Pollset");
		apr_socket_close(connection->sock);
		rtsp_connection_destroy(connection);
		return FALSE;
	}

	connection->it = apt_list_push_back(agent->connection_list,connection);

	apr_socket_addr_get(&connection->sockaddr,APR_REMOTE,connection->sock);
	if(apr_sockaddr_ip_get(&connection->remote_ip.buf,connection->sockaddr) == APR_SUCCESS) {
		connection->remote_ip.length = strlen(connection->remote_ip.buf);
	}
	apt_log(APT_PRIO_NOTICE,"Accepted RTSP Connection %s:%d",
			connection->remote_ip.buf,
			connection->sockaddr->port);
	if(agent->vtable && agent->vtable->on_connect) {
		agent->vtable->on_connect(connection);
	}
	return TRUE;
}

static apt_bool_t rtsp_server_agent_connection_close(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	apt_log(APT_PRIO_NOTICE,"RTSP Connection Disconnected");
	apr_pollset_remove(agent->pollset,&connection->sock_pfd);
	apr_socket_close(connection->sock);
	connection->sock = NULL;
	if(connection->access_count) {
		if(agent->vtable && agent->vtable->on_disconnect) {
			agent->vtable->on_disconnect(connection);
		}
	}
	else {
		rtsp_connection_remove(agent,connection);
		apt_log(APT_PRIO_NOTICE,"Destroy RTSP Connection");
		rtsp_connection_destroy(connection);
	}
	return TRUE;
}
static apt_bool_t rtsp_server_agent_connection_reference_add(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	connection->access_count++;
	return TRUE;
}

static apt_bool_t rtsp_server_agent_connection_reference_remove(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	if(!connection->access_count) {
		return FALSE;
	}

	connection->access_count--;
	if(!connection->access_count) {
		if(!connection->sock) {
			rtsp_connection_remove(agent,connection);
			apt_log(APT_PRIO_NOTICE,"Destroy RTSP Connection");
			rtsp_connection_destroy(connection);
		}
	}
	return TRUE;
}

static apt_bool_t rtsp_server_agent_messsage_send(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection, rtsp_message_t *message)
{
	apt_bool_t status = FALSE;
	if(connection && connection->sock) {
		char buffer[RTSP_MESSAGE_MAX_SIZE];
		apt_text_stream_t text_stream;
		
		text_stream.text.buf = buffer;
		text_stream.text.length = sizeof(buffer)-1;
		text_stream.pos = text_stream.text.buf;

		if(rtsp_message_generate(message,&text_stream) == TRUE) {
			*text_stream.pos = '\0';
			apt_log(APT_PRIO_INFO,"Send RTSP Message size=%lu\n%s",
				text_stream.text.length,text_stream.text.buf);
			if(apr_socket_send(connection->sock,text_stream.text.buf,&text_stream.text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_PRIO_WARNING,"Failed to Send RTSP Message");
			}
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Generate RTSP Message");
		}
	}
	else {
		apt_log(APT_PRIO_WARNING,"No RTSP Connection");
	}

	return status;
}

/** Send RTSP message receive event */
static APR_INLINE apt_bool_t rtsp_server_connection_message_receive(
						rtsp_server_agent_t *agent,
						rtsp_server_connection_t *connection, 
						rtsp_message_t *message)
{
	if(agent->vtable && agent->vtable->on_receive) {
		return agent->vtable->on_receive(connection,message);
	}
	return FALSE;
}

static apt_bool_t rtsp_server_agent_messsage_receive(rtsp_server_agent_t *agent, rtsp_server_connection_t *connection)
{
	char buffer[RTSP_MESSAGE_MAX_SIZE];
	apt_bool_t more_messages_on_buffer = FALSE;
	apr_status_t status;
	apt_text_stream_t text_stream;
	rtsp_message_t *message;

	if(!connection || !connection->sock) {
		return FALSE;
	}
	
	text_stream.text.buf = buffer;
	text_stream.text.length = sizeof(buffer)-1;
	status = apr_socket_recv(connection->sock, text_stream.text.buf, &text_stream.text.length);
	if(status == APR_EOF || text_stream.text.length == 0) {
		return rtsp_server_agent_connection_close(agent,connection);
	}
	text_stream.text.buf[text_stream.text.length] = '\0';
	text_stream.pos = text_stream.text.buf;

	apt_log(APT_PRIO_INFO,"Receive RTSP Message size=%lu\n%s",text_stream.text.length,text_stream.text.buf);
	do {
		message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,connection->pool);
		if(rtsp_message_parse(message,&text_stream) == TRUE) {
			rtsp_server_connection_message_receive(agent,connection,message);
		}
		else {
			rtsp_message_t *response;
			apt_log(APT_PRIO_WARNING,"Failed to Parse RTSP Message");
			response = rtsp_response_create(message,RTSP_STATUS_CODE_BAD_REQUEST,
									RTSP_REASON_PHRASE_BAD_REQUEST,message->pool);
			if(rtsp_server_agent_messsage_send(agent,connection,response) == FALSE) {
				apt_log(APT_PRIO_WARNING,"Failed to Send RTSP Response");
			}
		}

		more_messages_on_buffer = FALSE;
		if(text_stream.text.length > (apr_size_t)(text_stream.pos - text_stream.text.buf)) {
			/* there are more RTSP messages to signal */
			more_messages_on_buffer = TRUE;
			text_stream.text.length -= text_stream.pos - text_stream.text.buf;
			text_stream.text.buf = text_stream.pos;
			apt_log(APT_PRIO_DEBUG,"Saving Remaining Buffer for Next Message");
		}
	}
	while(more_messages_on_buffer);

	return TRUE;
}

static apt_bool_t rtsp_server_agent_control_pocess(rtsp_server_agent_t *agent)
{
	connection_task_msg_data_t task_msg_data;
	apr_size_t size = sizeof(connection_task_msg_data_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&task_msg_data, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	switch(task_msg_data.type) {
		case CONNECTION_TASK_MSG_ADD_REFERENCE:
			rtsp_server_agent_connection_reference_add(agent,task_msg_data.connection);
			break;
		case CONNECTION_TASK_MSG_REMOVE_REFERENCE:
			rtsp_server_agent_connection_reference_remove(agent,task_msg_data.connection);
			break;
		case CONNECTION_TASK_MSG_SEND_MESSAGE:
			rtsp_server_agent_messsage_send(agent,task_msg_data.connection,task_msg_data.message);
			break;
		case CONNECTION_TASK_MSG_TERMINATE:
			return FALSE;
	}

	return TRUE;
}

static apt_bool_t rtsp_server_agent_task_run(apt_task_t *task)
{
	rtsp_server_agent_t *agent = apt_task_object_get(task);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!agent) {
		apt_log(APT_PRIO_WARNING,"Failed to Start RTSP Agent");
		return FALSE;
	}

	if(rtsp_server_agent_pollset_create(agent) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	while(running) {
		status = apr_pollset_poll(agent->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(ret_pfd[i].desc.s == agent->listen_sock) {
				apt_log(APT_PRIO_DEBUG,"Accept RTSP Connection");
				rtsp_server_agent_connection_accept(agent);
				continue;
			}
			if(ret_pfd[i].desc.s == agent->control_sock) {
				apt_log(APT_PRIO_DEBUG,"Process Control Message");
				if(rtsp_server_agent_control_pocess(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_PRIO_DEBUG,"Process RTSP Message");
			rtsp_server_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	rtsp_server_agent_pollset_destroy(agent);

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t rtsp_server_agent_task_terminate(apt_task_t *task)
{
	apt_bool_t status = FALSE;
	rtsp_server_agent_t *agent = apt_task_object_get(task);
	if(agent->control_sock) {
		connection_task_msg_data_t data;
		apr_size_t size = sizeof(connection_task_msg_data_t);
		data.type = CONNECTION_TASK_MSG_TERMINATE;
		if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&data,&size) == APR_SUCCESS) {
			status = TRUE;
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Send Control Message");
		}
	}
	return status;
}
