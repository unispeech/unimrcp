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
#include "rtsp_client_connection.h"
#include "apt_text_stream.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define RTSP_MESSAGE_MAX_SIZE 2048


struct rtsp_client_agent_t {
	apr_pool_t              *pool;
	apt_task_t              *task;

	/** Server IP */
	apt_str_t                server_ip;
	/** Server Port */
	apr_port_t               server_port;

	apr_size_t               max_connection_count;
	apr_pollset_t           *pollset;

	/* Control socket */
	apr_sockaddr_t          *control_sockaddr;
	apr_socket_t            *control_sock;
	apr_pollfd_t             control_sock_pfd;

	apt_obj_list_t          *connection_list;

	void                                   *obj;
	const rtsp_client_agent_event_vtable_t *vtable;
};

/** RTSP connection */
struct rtsp_client_connection_t {
	/** Memory pool */
	apr_pool_t      *pool;

	/** Connected socket */
	apr_socket_t    *sock;
	/** Socket poll descriptor */
	apr_pollfd_t     sock_pfd;
	/** Remote sockaddr */
	apr_sockaddr_t  *sockaddr;

	/** Reference count */
	apr_size_t       access_count;
	/** Element of the connection list in agent */
	apt_list_elem_t *it;
};


typedef enum {
	CONNECTION_TASK_MSG_SEND_MESSAGE,
	CONNECTION_TASK_MSG_TERMINATE
} connection_task_msg_data_type_e;

typedef struct connection_task_msg_data_t connection_task_msg_data_t;
struct connection_task_msg_data_t {
	connection_task_msg_data_type_e type;
	rtsp_client_agent_t            *agent;
	rtsp_client_connection_t       *connection;
	rtsp_message_t                 *message;
};


static apt_bool_t rtsp_client_agent_task_run(apt_task_t *task);
static apt_bool_t rtsp_client_agent_task_terminate(apt_task_t *task);

/** Create connection agent. */
RTSP_DECLARE(rtsp_client_agent_t*) rtsp_client_connection_agent_create(
										const char *server_ip,
										apr_port_t server_port,
										apr_size_t max_connection_count,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	rtsp_client_agent_t *agent;
	
	apt_log(APT_PRIO_NOTICE,"Create RTSP Connection Agent [%d]",max_connection_count);
	agent = apr_palloc(pool,sizeof(rtsp_client_agent_t));
	agent->pool = pool;
	agent->control_sockaddr = NULL;
	agent->control_sock = NULL;
	agent->pollset = NULL;
	agent->max_connection_count = max_connection_count;

	apt_string_assign(&agent->server_ip,server_ip,pool);
	agent->server_port = server_port;

	apr_sockaddr_info_get(&agent->control_sockaddr,"127.0.0.1",APR_INET,7857,0,agent->pool);
	if(!agent->control_sockaddr) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = rtsp_client_agent_task_run;
	vtable.terminate = rtsp_client_agent_task_terminate;
	agent->task = apt_task_create(agent,&vtable,NULL,pool);
	if(!agent->task) {
		return NULL;
	}

	agent->connection_list = apt_list_create(pool);
	return agent;
}

/** Destroy connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_client_agent_destroy(rtsp_client_agent_t *agent)
{
	apt_log(APT_PRIO_NOTICE,"Destroy RTSP Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_client_agent_start(rtsp_client_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_client_agent_terminate(rtsp_client_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}

/** Set connection event handler. */
RTSP_DECLARE(void) rtsp_client_agent_handler_set(
									rtsp_client_agent_t *agent, 
									void *obj, 
									const rtsp_client_agent_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_client_agent_task_get(rtsp_client_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_client_agent_object_get(rtsp_client_agent_t *agent)
{
	return agent->obj;
}


static apt_bool_t rtsp_client_control_message_signal(
								connection_task_msg_data_type_e type,
								rtsp_client_agent_t *agent,
								rtsp_client_connection_t *connection,
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

/** Send RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_client_connection_message_send(rtsp_client_connection_t *connection, rtsp_message_t *message)
{
	return rtsp_client_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,NULL,connection,message);
}

static apt_bool_t rtsp_client_agent_control_socket_create(rtsp_client_agent_t *agent)
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

static void rtsp_client_agent_control_socket_destroy(rtsp_client_agent_t *agent)
{
	if(agent->control_sock) {
		apr_socket_close(agent->control_sock);
		agent->control_sock = NULL;
	}
}

static apt_bool_t rtsp_client_agent_pollset_create(rtsp_client_agent_t *agent)
{
	apr_status_t status;
	
	/* create pollset */
	status = apr_pollset_create(&agent->pollset, (apr_uint32_t)agent->max_connection_count + 1, agent->pool, 0);
	if(status != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* create control socket */
	if(rtsp_client_agent_control_socket_create(agent) != TRUE) {
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
		rtsp_client_agent_control_socket_destroy(agent);
		apr_pollset_destroy(agent->pollset);
		return FALSE;
	}

	return TRUE;
}

static void rtsp_client_agent_pollset_destroy(rtsp_client_agent_t *agent)
{
	rtsp_client_agent_control_socket_destroy(agent);
	if(agent->pollset) {
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
	}
}

static rtsp_client_connection_t* rtsp_connection_create()
{
	rtsp_client_connection_t *connection;
	apr_pool_t *pool;
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		return NULL;
	}
	
	connection = apr_palloc(pool,sizeof(rtsp_client_connection_t));
	connection->pool = pool;
	connection->sockaddr = NULL;
	connection->sock = NULL;
	connection->access_count = 0;
	connection->it = NULL;
	return connection;
}

static void rtsp_connection_destroy(rtsp_client_connection_t *connection)
{
	if(connection && connection->pool) {
		apr_pool_destroy(connection->pool);
	}
}

static rtsp_client_connection_t* rtsp_client_agent_connection_create(rtsp_client_agent_t *agent)
{
	rtsp_client_connection_t *connection = rtsp_connection_create();

	apr_sockaddr_info_get(&connection->sockaddr,agent->server_ip.buf,APR_INET,agent->server_port,0,connection->pool);
	if(!connection->sockaddr) {
		rtsp_connection_destroy(connection);
		return NULL;
	}

	if(apr_socket_create(&connection->sock, connection->sockaddr->family, SOCK_STREAM, APR_PROTO_TCP, connection->pool) != APR_SUCCESS) {
		rtsp_connection_destroy(connection);
		return NULL;
	}

	apr_socket_opt_set(connection->sock, APR_SO_NONBLOCK, 0);
	apr_socket_timeout_set(connection->sock, -1);
	apr_socket_opt_set(connection->sock, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(connection->sock, connection->sockaddr) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		rtsp_connection_destroy(connection);
		return NULL;
	}

	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apr_pollset_add(agent->pollset, &connection->sock_pfd) != APR_SUCCESS) {
		apr_socket_close(connection->sock);
		rtsp_connection_destroy(connection);
		return NULL;
	}
	
	apt_log(APT_PRIO_NOTICE,"Established RTSP Connection %s:%d",
			agent->server_ip.buf,
			agent->server_port);
	connection->it = apt_list_push_back(agent->connection_list,connection);
	return connection;
}

static apt_bool_t rtsp_client_agent_connection_remove(rtsp_client_agent_t *agent, rtsp_client_connection_t *connection)
{
	/* remove from the list */
	if(connection->it) {
		apt_list_elem_remove(agent->connection_list,connection->it);
		connection->it = NULL;
	}
	apr_pollset_remove(agent->pollset,&connection->sock_pfd);
	if(connection->sock) {
		apr_socket_close(connection->sock);
		connection->sock = NULL;
	}
	apt_log(APT_PRIO_NOTICE,"Disconnected RTSP Connection");
	return TRUE;
}

/** Send RTSP message receive event */
static APR_INLINE apt_bool_t rtsp_client_connection_message_receive(
						rtsp_client_agent_t *agent,
						rtsp_client_connection_t *connection, 
						rtsp_message_t *message)
{
	if(agent->vtable && agent->vtable->on_receive) {
		return agent->vtable->on_receive(connection,message);
	}
	return FALSE;
}

static apt_bool_t rtsp_client_agent_messsage_send(rtsp_client_agent_t *agent, rtsp_client_connection_t *connection, rtsp_message_t *message)
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

	if(status == FALSE) {
		rtsp_message_t *response;
		apt_log(APT_PRIO_WARNING,"Failed to Send RTSP Message");
		response = rtsp_response_create(
							message,
							RTSP_STATUS_CODE_BAD_REQUEST,
							RTSP_REASON_PHRASE_BAD_REQUEST,
							message->pool);
		rtsp_client_connection_message_receive(agent,connection,response);
	}
	return TRUE;
}

 static apt_bool_t rtsp_client_agent_messsage_receive(rtsp_client_agent_t *agent, rtsp_client_connection_t *connection)
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
		apt_log(APT_PRIO_NOTICE,"RTSP Connection Disconnected");
		apr_pollset_remove(agent->pollset,&connection->sock_pfd);
		apr_socket_close(connection->sock);
		connection->sock = NULL;

//		agent->vtable->on_disconnect(agent,connection);
		return TRUE;
	}
	text_stream.text.buf[text_stream.text.length] = '\0';
	text_stream.pos = text_stream.text.buf;

	apt_log(APT_PRIO_INFO,"Receive RTSP Message size=%lu\n%s",text_stream.text.length,text_stream.text.buf);
	if(!connection->access_count) {
		return FALSE;
	}

	do {
		message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,connection->pool);
		if(rtsp_message_parse(message,&text_stream) == TRUE) {
			rtsp_client_connection_message_receive(agent,connection,message);
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Parse RTSP Message");
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

static apt_bool_t rtsp_client_agent_control_pocess(rtsp_client_agent_t *agent)
{
	connection_task_msg_data_t task_msg_data;
	apr_size_t size = sizeof(connection_task_msg_data_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&task_msg_data, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	switch(task_msg_data.type) {
		case CONNECTION_TASK_MSG_SEND_MESSAGE:
			rtsp_client_agent_messsage_send(agent,task_msg_data.connection,task_msg_data.message);
			break;
		case CONNECTION_TASK_MSG_TERMINATE:
			return FALSE;
	}

	return TRUE;
}

static apt_bool_t rtsp_client_agent_task_run(apt_task_t *task)
{
	rtsp_client_agent_t *agent = apt_task_object_get(task);
	apt_bool_t running = TRUE;
	apr_status_t status;
	apr_int32_t num;
	const apr_pollfd_t *ret_pfd;
	int i;

	if(!agent) {
		apt_log(APT_PRIO_WARNING,"Failed to Start RTSP Agent");
		return FALSE;
	}

	if(rtsp_client_agent_pollset_create(agent) == FALSE) {
		apt_log(APT_PRIO_WARNING,"Failed to Create RTSP Agent Socket");
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
				if(rtsp_client_agent_control_pocess(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			rtsp_client_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	rtsp_client_agent_pollset_destroy(agent);

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t rtsp_client_agent_task_terminate(apt_task_t *task)
{
	apt_bool_t status = FALSE;
	rtsp_client_agent_t *agent = apt_task_object_get(task);
	if(agent->control_sock) {
		connection_task_msg_data_t task_msg_data;
		apr_size_t size = sizeof(connection_task_msg_data_t);
		task_msg_data.type = CONNECTION_TASK_MSG_TERMINATE;
		if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&task_msg_data,&size) == APR_SUCCESS) {
			status = TRUE;
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Send Control Message");
		}
	}
	return status;
}
