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

#include <apr_hash.h>
#include "rtsp_server.h"
#include "apt_net_server_task.h"
#include "apt_text_stream.h"
#include "apt_obj_list.h"
#include "apt_log.h"

#define RTSP_SESSION_ID_HEX_STRING_LENGTH 16
#define RTSP_MESSAGE_MAX_SIZE 2048

typedef struct rtsp_server_connection_t rtsp_server_connection_t;

/** RTSP server */
struct rtsp_server_t {
	apr_pool_t                 *pool;
	apt_net_server_task_t      *task;

	apr_pool_t                 *sub_pool;
	apt_obj_list_t             *connection_list;

	void                       *obj;
	rtsp_server_event_handler_f handler;
};

/** RTSP connection */
struct rtsp_server_connection_t {
	/** Connection base */
	apt_net_server_connection_t *base;

	/** Element of the connection list in agent */
	apt_list_elem_t             *it;

	/** Session table (rtsp_server_session_t*) */
	apr_hash_t                  *session_table;
};

/** RTSP session */
struct rtsp_server_session_t {
	apr_pool_t               *pool;
	void                     *obj;
	rtsp_server_connection_t *connection;

	/** Session identifier */
	apt_str_t                 id;
	apt_str_t                 url;

	/** In-progress request */
	rtsp_message_t           *active_request;
	/** request queue */
	apt_obj_list_t           *request_queue;
};

typedef enum {
	TASK_MSG_SEND_MESSAGE,
} task_msg_data_type_e;

typedef struct task_msg_data_t task_msg_data_t;

struct task_msg_data_t {
	task_msg_data_type_e   type;
	rtsp_server_t         *server;
	rtsp_server_session_t *session;
	rtsp_message_t        *message;
};

static apt_bool_t rtsp_server_task_msg_process(apt_task_t *task, apt_task_msg_t *msg);

static apt_bool_t rtsp_server_on_connect(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
static apt_bool_t rtsp_server_on_disconnect(apt_net_server_task_t *task, apt_net_server_connection_t *connection);
static apt_bool_t rtsp_server_on_receive(apt_net_server_task_t *task, apt_net_server_connection_t *connection);

static const apt_net_server_vtable_t server_vtable = {
	rtsp_server_on_connect,
	rtsp_server_on_disconnect,
	rtsp_server_on_receive
};

static apt_bool_t rtsp_server_do_send(rtsp_server_t *server, apt_net_server_connection_t *connection, rtsp_message_t *message);

/** Create RTSP server */
RTSP_DECLARE(rtsp_server_t*) rtsp_server_create(
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	rtsp_server_t *server;
	
	apt_log(APT_PRIO_NOTICE,"Create RTSP Server %s:%hu [%d]",listen_ip,listen_port,max_connection_count);
	server = apr_palloc(pool,sizeof(rtsp_server_t));
	server->pool = pool;

	msg_pool = apt_task_msg_pool_create_dynamic(sizeof(task_msg_data_t),pool);

	apt_task_vtable_reset(&vtable);
	vtable.process_msg = rtsp_server_task_msg_process;
	server->task = apt_net_server_task_create(
						listen_ip,listen_port,max_connection_count,
						server,&vtable,&server_vtable,msg_pool,pool);
	if(!server->task) {
		return NULL;
	}

	apr_pool_create(&server->sub_pool,pool);
	server->connection_list = NULL;
	return server;
}

/** Destroy RTSP server */
RTSP_DECLARE(apt_bool_t) rtsp_server_destroy(rtsp_server_t *server)
{
	apt_log(APT_PRIO_NOTICE,"Destroy RTSP Server");
	return apt_net_server_task_destroy(server->task);
}

/** Start connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_start(rtsp_server_t *server)
{
	return apt_net_server_task_start(server->task);
}

/** Terminate connection agent. */
RTSP_DECLARE(apt_bool_t) rtsp_server_terminate(rtsp_server_t *server)
{
	return apt_net_server_task_terminate(server->task);
}

/** Set connection event handler. */
RTSP_DECLARE(void) rtsp_server_event_handler_set(
									rtsp_server_t *server,
									void *obj, 
									rtsp_server_event_handler_f handler)
{
	server->obj = obj;
	server->handler = handler;
}

/** Get task */
RTSP_DECLARE(apt_task_t*) rtsp_serve_task_get(rtsp_server_t *server)
{
	return apt_net_server_task_object_get(server->task);
}

/** Get external object */
RTSP_DECLARE(void*) rtsp_server_object_get(rtsp_server_t *server)
{
	return server->obj;
}

/** Get object associated with the session */
RTSP_DECLARE(void*) rtsp_server_session_object_get(const rtsp_server_session_t *session)
{
	return session->obj;
}

/** Set object associated with the session */
RTSP_DECLARE(void) rtsp_server_session_object_set(rtsp_server_session_t *session, void *obj)
{
	session->obj = obj;
}

/** Get the session identifier */
RTSP_DECLARE(const apt_str_t*) rtsp_server_session_id_get(const rtsp_server_session_t *session)
{
	return &session->id;
}

RTSP_DECLARE(const rtsp_message_t*) rtsp_server_session_request_get(const rtsp_server_session_t *session)
{
	return session->active_request;
}

static apt_bool_t rtsp_server_control_message_signal(
								task_msg_data_type_e type,
								rtsp_server_t *server,
								rtsp_server_session_t *session,
								rtsp_message_t *message)
{
	apt_task_t *task = apt_net_server_task_base_get(server->task);
	apt_task_msg_t *task_msg = apt_task_msg_get(task);
	if(task_msg) {
		task_msg_data_t *data = (task_msg_data_t*)task_msg->data;
		data->type = type;
		data->server = server;
		data->session = session;
		data->message = message;
		apt_task_msg_signal(task,task_msg);
	}
	return TRUE;
}

/** Send RTSP message */
RTSP_DECLARE(apt_bool_t) rtsp_server_message_send(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message)
{
	return rtsp_server_control_message_signal(TASK_MSG_SEND_MESSAGE,server,session,message);
}

static rtsp_server_session_t* rtsp_server_session_create()
{
	rtsp_server_session_t *session;
	apr_pool_t *pool;
	apr_pool_create(&pool,NULL);
	session = apr_palloc(pool,sizeof(rtsp_server_session_t));
	session->pool = pool;
	session->obj = NULL;
	session->active_request = NULL;
	session->request_queue = apt_list_create(pool);

	apt_string_reset(&session->url);
	apt_unique_id_generate(&session->id,RTSP_SESSION_ID_HEX_STRING_LENGTH,pool);
	apt_log(APT_PRIO_NOTICE,"Create RTSP Session <%s>",session->id.buf);
	return session;
}

static void rtsp_server_session_destroy(rtsp_server_session_t *session)
{
	apt_log(APT_PRIO_NOTICE,"Destroy RTSP Session <%s>",session->id.buf);
	if(session && session->pool) {
		apr_pool_destroy(session->pool);
	}
}

static void rtsp_server_session_teardown(rtsp_server_t *server, rtsp_server_session_t *session)
{
	rtsp_message_t *message = rtsp_request_create(session->pool);
	message->start_line.common.request_line.method_id = RTSP_METHOD_TEARDOWN;

	if(session->active_request) {
		apt_log(APT_PRIO_DEBUG,"Push Teardown Request to Queue");
		apt_list_push_back(session->request_queue,message);
	}
	else {
		session->active_request = message;
		server->handler(server,session,message);
	}
}

static apt_bool_t rtsp_server_message_receive_process(rtsp_server_t *server, rtsp_server_connection_t *rtsp_connection, rtsp_message_t *message)
{
	rtsp_server_session_t *session = NULL;
	if(rtsp_header_property_check(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID) == TRUE) {
		/* existing session */
		session = apr_hash_get(
					rtsp_connection->session_table,
					message->header.session_id.buf,
					message->header.session_id.length);
		if(!session) {
			/* error case */
		}
	}
	else {
		if(message->start_line.message_type == RTSP_MESSAGE_TYPE_REQUEST) {
			if(message->start_line.common.request_line.method_id == RTSP_METHOD_SETUP) {
				/* create new session */
				session = rtsp_server_session_create();
				session->connection = rtsp_connection;
				session->url = message->start_line.common.request_line.url;
				apt_log(APT_PRIO_INFO,"Add RTSP Session <%s>",session->id.buf);
				apr_hash_set(rtsp_connection->session_table,session->id.buf,session->id.length,session);
			}
			else if(message->start_line.common.request_line.method_id == RTSP_METHOD_DESCRIBE) {
				/* create new session as a communication object */
				session = rtsp_server_session_create();
				session->connection = rtsp_connection;
			}
			else {
				/* error case */
			}
		}
		else {
			/* error case */
		}
	}
	
	if(session) {
		if(session->active_request) {
			apt_log(APT_PRIO_DEBUG,"Push RTSP Request to Queue");
			apt_list_push_back(session->request_queue,message);
		}
		else {
			/* send offer to application */
			session->active_request = message;
			server->handler(server,session,message);
		}
	}
	else {
		/* send error response to client */
		rtsp_message_t *response = rtsp_response_create(message,RTSP_STATUS_CODE_BAD_REQUEST,
									RTSP_REASON_PHRASE_BAD_REQUEST,message->pool);
		if(rtsp_server_do_send(server,rtsp_connection->base,response) == FALSE) {
			apt_log(APT_PRIO_WARNING,"Failed to Send RTSP Response");
		}
	}
	return TRUE;
}

static apt_bool_t rtsp_server_message_send_process(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message)
{
	apt_bool_t destroy_session = FALSE;
	if(message->start_line.message_type == RTSP_MESSAGE_TYPE_RESPONSE) {
		if(session->active_request) {
			if(session->active_request->start_line.common.request_line.method_id == RTSP_METHOD_TEARDOWN) {
				apt_log(APT_PRIO_INFO,"Remove RTSP Session <%s>",session->id.buf);
				apr_hash_set(session->connection->session_table,session->id.buf,session->id.length,NULL);
				destroy_session = TRUE;
			}
			else if(session->active_request->start_line.common.request_line.method_id == RTSP_METHOD_DESCRIBE) {
				destroy_session = TRUE;
			}
		}
	}
	else if(message->start_line.message_type == RTSP_MESSAGE_TYPE_REQUEST) {
		/* RTSP Announce */
		message->start_line.common.request_line.url = session->url;
	}
	
	if(session->id.buf) {
		message->header.session_id = session->id;
		rtsp_header_property_add(&message->header.property_set,RTSP_HEADER_FIELD_SESSION_ID);
	}

	rtsp_server_do_send(server,session->connection->base,message);

	session->active_request = apt_list_pop_front(session->request_queue);
	if(session->active_request) {
		server->handler(server,session,session->active_request);
	}

	if(destroy_session == TRUE) {
		rtsp_server_connection_t *rtsp_connection = session->connection;
		rtsp_server_session_destroy(session);

		if(rtsp_connection && !rtsp_connection->it) {
			if(apr_hash_count(rtsp_connection->session_table) == 0) {
				apt_net_server_connection_destroy(rtsp_connection->base);
			}
		}
	}
	return TRUE;
}

static apt_bool_t rtsp_server_do_send(rtsp_server_t *server, apt_net_server_connection_t *connection, rtsp_message_t *message)
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

static apt_bool_t rtsp_server_on_receive(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	rtsp_server_t *server = apt_net_server_task_object_get(task);
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
		return apt_net_server_connection_close(task,connection);
	}
	text_stream.text.buf[text_stream.text.length] = '\0';
	text_stream.pos = text_stream.text.buf;

	apt_log(APT_PRIO_INFO,"Receive RTSP Message size=%lu\n%s",text_stream.text.length,text_stream.text.buf);
	do {
		message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,connection->pool);
		if(rtsp_message_parse(message,&text_stream) == TRUE) {
			rtsp_server_message_receive_process(server,connection->obj,message);
		}
		else {
			rtsp_message_t *response;
			apt_log(APT_PRIO_WARNING,"Failed to Parse RTSP Message");
			response = rtsp_response_create(message,RTSP_STATUS_CODE_BAD_REQUEST,
									RTSP_REASON_PHRASE_BAD_REQUEST,message->pool);
			if(rtsp_server_do_send(server,connection,response) == FALSE) {
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

static apt_bool_t rtsp_server_on_connect(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	rtsp_server_t *server = apt_net_server_task_object_get(task);
	rtsp_server_connection_t *rtsp_connection = apr_palloc(connection->pool,sizeof(rtsp_server_connection_t));
	rtsp_connection->session_table = apr_hash_make(connection->pool);
	rtsp_connection->base = connection;
	connection->obj = rtsp_connection;
	if(!server->connection_list) {
		server->connection_list = apt_list_create(server->sub_pool);
	}
	rtsp_connection->it = apt_list_push_back(server->connection_list,rtsp_connection);
	return TRUE;
}

static apt_bool_t rtsp_server_on_disconnect(apt_net_server_task_t *task, apt_net_server_connection_t *connection)
{
	apr_size_t remaining_sessions = 0;
	rtsp_server_t *server = apt_net_server_task_object_get(task);
	rtsp_server_connection_t *rtsp_connection = connection->obj;
	apt_list_elem_remove(server->connection_list,rtsp_connection->it);
	rtsp_connection->it = NULL;
	if(apt_list_is_empty(server->connection_list) == TRUE) {
		apr_pool_clear(server->sub_pool);
		server->connection_list = NULL;
	}

	remaining_sessions = apr_hash_count(rtsp_connection->session_table);
	if(remaining_sessions) {
		rtsp_server_session_t *session;
		void *val;
		apr_hash_index_t *it;
		apt_log(APT_PRIO_NOTICE,"Teardown Remaining Sessions [%d]",remaining_sessions);
		it = apr_hash_first(connection->pool,rtsp_connection->session_table);
		for(; it; it = apr_hash_next(it)) {
			apr_hash_this(it,NULL,NULL,&val);
			session = val;
			if(session) {
				rtsp_server_session_teardown(server,session);
			}
		}
	}
	else {
		apt_net_server_connection_destroy(connection);
	}
	return TRUE;
}

static apt_bool_t rtsp_server_task_msg_process(apt_task_t *task, apt_task_msg_t *task_msg)
{
	apt_net_server_task_t *net_task = apt_task_object_get(task);
	rtsp_server_t *server = apt_net_server_task_object_get(net_task);

	task_msg_data_t *data = (task_msg_data_t*) task_msg->data;
	switch(data->type) {
		case TASK_MSG_SEND_MESSAGE:
			rtsp_server_message_send_process(server,data->session,data->message);
			break;
	}

	return TRUE;
}
