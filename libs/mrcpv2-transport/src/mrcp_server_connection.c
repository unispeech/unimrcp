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

#include "mrcp_connection.h"
#include "mrcp_server_connection.h"
#include "mrcp_control_descriptor.h"
#include "mrcp_resource_factory.h"
#include "mrcp_message.h"
#include "apt_text_stream.h"
#include "apt_task.h"
#include "apt_log.h"

struct mrcp_connection_agent_t {
	apr_pool_t              *pool;
	apt_task_t              *task;
	mrcp_resource_factory_t *resource_factory;

	apt_bool_t               force_new_connection;

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
	mrcp_connection_t       *null_connection;

	void                                 *obj;
	const mrcp_connection_event_vtable_t *vtable;
};

typedef enum {
	CONNECTION_TASK_MSG_ADD_CHANNEL,
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
MRCP_DECLARE(mrcp_connection_agent_t*) mrcp_server_connection_agent_create(
										const char *listen_ip,
										apr_port_t listen_port,
										apr_size_t max_connection_count,
										apt_bool_t force_new_connection,
										apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_connection_agent_t *agent;

	if(!listen_ip) {
		return NULL;
	}
	
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create TCP/MRCPv2 Connection Agent %s:%hu [%d]",listen_ip,listen_port,max_connection_count);
	agent = apr_palloc(pool,sizeof(mrcp_connection_agent_t));
	agent->pool = pool;
	agent->sockaddr = NULL;
	agent->listen_sock = NULL;
	agent->control_sockaddr = NULL;
	agent->control_sock = NULL;
	agent->pollset = NULL;
	agent->max_connection_count = max_connection_count;
	agent->force_new_connection = force_new_connection;

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

	agent->connection_list = NULL;
	agent->null_connection = NULL;
	return agent;
}

/** Destroy connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_destroy(mrcp_connection_agent_t *agent)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy MRCPv2 Agent");
	return apt_task_destroy(agent->task);
}

/** Start connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_start(mrcp_connection_agent_t *agent)
{
	return apt_task_start(agent->task);
}

/** Terminate connection agent. */
MRCP_DECLARE(apt_bool_t) mrcp_server_connection_agent_terminate(mrcp_connection_agent_t *agent)
{
	return apt_task_terminate(agent->task,TRUE);
}

/** Set connection event handler. */
MRCP_DECLARE(void) mrcp_server_connection_agent_handler_set(
									mrcp_connection_agent_t *agent, 
									void *obj, 
									const mrcp_connection_event_vtable_t *vtable)
{
	agent->obj = obj;
	agent->vtable = vtable;
}

/** Set MRCP resource factory */
MRCP_DECLARE(void) mrcp_server_connection_resource_factory_set(
								mrcp_connection_agent_t *agent, 
								mrcp_resource_factory_t *resource_factroy)
{
	agent->resource_factory = resource_factroy;
}

/** Get task */
MRCP_DECLARE(apt_task_t*) mrcp_server_connection_agent_task_get(mrcp_connection_agent_t *agent)
{
	return agent->task;
}

/** Get external object */
MRCP_DECLARE(void*) mrcp_server_connection_agent_object_get(mrcp_connection_agent_t *agent)
{
	return agent->obj;
}

/** Create MRCPv2 control channel */
MRCP_DECLARE(mrcp_control_channel_t*) mrcp_server_control_channel_create(mrcp_connection_agent_t *agent, void *obj, apr_pool_t *pool)
{
	mrcp_control_channel_t *channel = apr_palloc(pool,sizeof(mrcp_control_channel_t));
	channel->agent = agent;
	channel->connection = NULL;
	channel->removed = FALSE;
	channel->obj = obj;
	channel->pool = pool;
	return channel;
}

/** Destroy MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_destroy(mrcp_control_channel_t *channel)
{
	if(channel && channel->connection && channel->removed == TRUE) {
		mrcp_connection_t *connection = channel->connection;
		channel->connection = NULL;
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP/MRCPv2 Connection");
		mrcp_connection_destroy(connection);
	}
	return TRUE;
}

static apt_bool_t mrcp_server_control_message_signal(
								connection_task_msg_data_type_e type,
								mrcp_connection_agent_t *agent,
								mrcp_control_channel_t *channel,
								mrcp_control_descriptor_t *descriptor,
								mrcp_message_t *message)
{
	apr_size_t size = sizeof(connection_task_msg_data_t);
	connection_task_msg_data_t data;
	if(!agent->control_sock) {
		return FALSE;
	}
	memset(&data,0,size);
	data.type = type;
	data.agent = agent;
	data.channel = channel;
	data.descriptor = descriptor;
	data.message = message;

	if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&data,&size) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Signal Control Message");
		return FALSE;
	}
	return TRUE;
}

/** Add MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_add(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_ADD_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Modify MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_modify(mrcp_control_channel_t *channel, mrcp_control_descriptor_t *descriptor)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_MODIFY_CHANNEL,channel->agent,channel,descriptor,NULL);
}

/** Remove MRCPv2 control channel */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_channel_remove(mrcp_control_channel_t *channel)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_REMOVE_CHANNEL,channel->agent,channel,NULL,NULL);
}

/** Send MRCPv2 message */
MRCP_DECLARE(apt_bool_t) mrcp_server_control_message_send(mrcp_control_channel_t *channel, mrcp_message_t *message)
{
	return mrcp_server_control_message_signal(CONNECTION_TASK_MSG_SEND_MESSAGE,channel->agent,channel,NULL,message);
}


static apt_bool_t mrcp_server_agent_listen_socket_create(mrcp_connection_agent_t *agent)
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

static void mrcp_server_agent_listen_socket_destroy(mrcp_connection_agent_t *agent)
{
	if(agent->listen_sock) {
		apr_socket_close(agent->listen_sock);
		agent->listen_sock = NULL;
	}
}

static apt_bool_t mrcp_server_agent_control_socket_create(mrcp_connection_agent_t *agent)
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

static void mrcp_server_agent_control_socket_destroy(mrcp_connection_agent_t *agent)
{
	if(agent->control_sock) {
		apr_socket_close(agent->control_sock);
		agent->control_sock = NULL;
	}
}

static apt_bool_t mrcp_server_agent_pollset_create(mrcp_connection_agent_t *agent)
{
	apr_status_t status;
	/* create pollset */
	status = apr_pollset_create(&agent->pollset, (apr_uint32_t)agent->max_connection_count + 2, agent->pool, 0);
	if(status != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	/* create control socket */
	if(mrcp_server_agent_control_socket_create(agent) != TRUE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Control Socket");
		apr_pollset_destroy(agent->pollset);
		return FALSE;
	}
	/* add control socket to pollset */
	memset(&agent->control_sock_pfd,0,sizeof(apr_pollfd_t));
	agent->control_sock_pfd.desc_type = APR_POLL_SOCKET;
	agent->control_sock_pfd.reqevents = APR_POLLIN;
	agent->control_sock_pfd.desc.s = agent->control_sock;
	agent->control_sock_pfd.client_data = agent->control_sock;
	status = apr_pollset_add(agent->pollset, &agent->control_sock_pfd);
	if(status != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add Control Socket to Pollset");
		mrcp_server_agent_control_socket_destroy(agent);
		apr_pollset_destroy(agent->pollset);
		return FALSE;
	}

	if(mrcp_server_agent_listen_socket_create(agent) == TRUE) {
		/* add listen socket to pollset */
		memset(&agent->listen_sock_pfd,0,sizeof(apr_pollfd_t));
		agent->listen_sock_pfd.desc_type = APR_POLL_SOCKET;
		agent->listen_sock_pfd.reqevents = APR_POLLIN;
		agent->listen_sock_pfd.desc.s = agent->listen_sock;
		agent->listen_sock_pfd.client_data = agent->listen_sock;
		status = apr_pollset_add(agent->pollset, &agent->listen_sock_pfd);
		if(status != APR_SUCCESS) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add Listen Socket to Pollset");
			mrcp_server_agent_listen_socket_destroy(agent);
		}
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Listen Socket");
	}

	return TRUE;
}

static void mrcp_server_agent_pollset_destroy(mrcp_connection_agent_t *agent)
{
	mrcp_server_agent_listen_socket_destroy(agent);
	mrcp_server_agent_control_socket_destroy(agent);
	if(agent->pollset) {
		apr_pollset_destroy(agent->pollset);
		agent->pollset = NULL;
	}
}


static mrcp_control_channel_t* mrcp_connection_channel_associate(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, const mrcp_message_t *message)
{
	apt_str_t identifier;
	mrcp_control_channel_t *channel;
	if(!connection || !message) {
		return NULL;
	}
	apt_id_resource_generate(&message->channel_id.session_id,&message->channel_id.resource_name,'@',&identifier,connection->pool);
	channel = mrcp_connection_channel_find(connection,&identifier);
	if(!channel) {
		channel = mrcp_connection_channel_find(agent->null_connection,&identifier);
		if(channel) {
			mrcp_connection_channel_remove(agent->null_connection,channel);
			mrcp_connection_channel_add(connection,channel);
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Move Control Channel <%s> to Connection [%d]",
				channel->identifier.buf,
				apr_hash_count(connection->channel_table));
		}
	}
	return channel;
}

static mrcp_connection_t* mrcp_connection_find(mrcp_connection_agent_t *agent, const apt_str_t *remote_ip)
{
	mrcp_connection_t *connection = NULL;
	apt_list_elem_t *elem;
	if(!agent || !agent->connection_list || !remote_ip) {
		return NULL;
	}

	elem = apt_list_first_elem_get(agent->connection_list);
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

static apt_bool_t mrcp_connection_remove(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	if(connection->it) {
		apt_list_elem_remove(agent->connection_list,connection->it);
		connection->it = NULL;
	}
	if(agent->null_connection) {
		if(apt_list_is_empty(agent->connection_list) == TRUE && apr_hash_count(agent->null_connection->channel_table) == 0) {
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Pending Connection");
			mrcp_connection_destroy(agent->null_connection);
			agent->null_connection = NULL;
			agent->connection_list = NULL;
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_agent_connection_accept(mrcp_connection_agent_t *agent)
{
	apr_socket_t *sock;
	apr_pool_t *pool;
	mrcp_connection_t *connection;

	if(!agent->null_connection) {
		apr_pool_create(&pool,NULL);
		if(apr_socket_accept(&sock,agent->listen_sock,pool) != APR_SUCCESS) {
			return FALSE;
		}
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Rejected TCP/MRCPv2 Connection");
		apr_socket_close(sock);
		apr_pool_destroy(pool);
		return FALSE;
	}

	pool = agent->null_connection->pool;
	if(apr_socket_accept(&sock,agent->listen_sock,pool) != APR_SUCCESS) {
		return FALSE;
	}

	connection = mrcp_connection_create();
	connection->sock = sock;
	memset(&connection->sock_pfd,0,sizeof(apr_pollfd_t));
	connection->sock_pfd.desc_type = APR_POLL_SOCKET;
	connection->sock_pfd.reqevents = APR_POLLIN;
	connection->sock_pfd.desc.s = connection->sock;
	connection->sock_pfd.client_data = connection;
	if(apr_pollset_add(agent->pollset, &connection->sock_pfd) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Add to Pollset");
		apr_socket_close(sock);
		mrcp_connection_destroy(connection);
		return FALSE;
	}

	connection->agent = agent;
	connection->it = apt_list_push_back(agent->connection_list,connection,connection->pool);
	connection->parser = mrcp_parser_create(agent->resource_factory,connection->pool);
	connection->generator = mrcp_generator_create(agent->resource_factory,connection->pool);

	apr_socket_addr_get(&connection->sockaddr,APR_REMOTE,sock);
	if(apr_sockaddr_ip_get(&connection->remote_ip.buf,connection->sockaddr) == APR_SUCCESS) {
		connection->remote_ip.length = strlen(connection->remote_ip.buf);
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Accepted TCP/MRCPv2 Connection %s:%d",
			connection->remote_ip.buf,
			connection->sockaddr->port);
	return TRUE;
}

static apt_bool_t mrcp_server_agent_connection_close(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"TCP/MRCPv2 Connection Disconnected");
	apr_pollset_remove(agent->pollset,&connection->sock_pfd);
	apr_socket_close(connection->sock);
	connection->sock = NULL;
	if(!connection->access_count) {
		mrcp_connection_remove(agent,connection);
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy TCP/MRCPv2 Connection");
		mrcp_connection_destroy(connection);
	}
	return TRUE;
}

static apt_bool_t mrcp_server_agent_channel_add(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *offer)
{
	mrcp_control_descriptor_t *answer = mrcp_control_answer_create(offer,channel->pool);
	apt_id_resource_generate(&offer->session_id,&offer->resource_name,'@',&channel->identifier,channel->pool);
	if(offer->port) {
		answer->port = agent->sockaddr->port;
	}
	if(offer->connection_type == MRCP_CONNECTION_TYPE_EXISTING) {
		if(agent->force_new_connection == TRUE) {
			/* force client to establish new connection */
			answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
		}
		else {
			mrcp_connection_t *connection = NULL;
			/* try to find any existing connection */
			connection = mrcp_connection_find(agent,&offer->ip);
			if(!connection) {
				/* no existing conection found, force the new one */
				answer->connection_type = MRCP_CONNECTION_TYPE_NEW;
			}
		}
	}

	if(!agent->null_connection) {
		apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Create Pending Connection");
		agent->null_connection = mrcp_connection_create();
		agent->connection_list = apt_list_create(agent->null_connection->pool);
	}
	mrcp_connection_channel_add(agent->null_connection,channel);	
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Control Channel <%s> to Pending Connection [%d]",
			channel->identifier.buf,
			apr_hash_count(agent->null_connection->channel_table));
	/* send response */
	return mrcp_control_channel_add_respond(agent->vtable,channel,answer,TRUE);
}

static apt_bool_t mrcp_server_agent_channel_modify(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel, mrcp_control_descriptor_t *offer)
{
	mrcp_control_descriptor_t *answer = mrcp_control_answer_create(offer,channel->pool);
	if(offer->port) {
		answer->port = agent->sockaddr->port;
	}
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Modify Control Channel <%s>",channel->identifier.buf);
	/* send response */
	return mrcp_control_channel_modify_respond(agent->vtable,channel,answer,TRUE);
}

static apt_bool_t mrcp_server_agent_channel_remove(mrcp_connection_agent_t *agent, mrcp_control_channel_t *channel)
{
	mrcp_connection_t *connection = channel->connection;
	mrcp_connection_channel_remove(connection,channel);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Remove Control Channel <%s> [%d]",
			channel->identifier.buf,
			apr_hash_count(connection->channel_table));
	if(!connection->access_count) {
		if(connection == agent->null_connection) {
			if(apt_list_is_empty(agent->connection_list) == TRUE) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Destroy Pending Connection");
				mrcp_connection_destroy(agent->null_connection);
				agent->null_connection = NULL;
				agent->connection_list = NULL;
			}
		}
		else if(!connection->sock) {
			mrcp_connection_remove(agent,connection);
			/* set connection to be destroyed on channel destroy */
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Mark Connection for Late Destroy");
			channel->connection = connection;
			channel->removed = TRUE;
		}
	}
	/* send response */
	return mrcp_control_channel_remove_respond(agent->vtable,channel,TRUE);
}

static apt_bool_t mrcp_server_agent_messsage_send(mrcp_connection_agent_t *agent, mrcp_connection_t *connection, mrcp_message_t *message)
{
	apt_bool_t status = FALSE;
	apt_text_stream_t *stream;
	mrcp_stream_result_e result;
	if(!connection || !connection->sock) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No MRCPv2 Connection");
		return FALSE;
	}
	stream = &connection->tx_stream;

	mrcp_generator_message_set(connection->generator,message);
	do {
		apt_text_stream_init(&connection->tx_stream,connection->tx_buffer,sizeof(connection->tx_buffer)-1);
		result = mrcp_generator_run(connection->generator,stream);
		if(result == MRCP_STREAM_MESSAGE_COMPLETE || result == MRCP_STREAM_MESSAGE_TRUNCATED) {
			stream->text.length = stream->pos - stream->text.buf;
			*stream->pos = '\0';

			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Send MRCPv2 Stream [%lu bytes]\n%s",
				stream->text.length,stream->text.buf);
			if(apr_socket_send(connection->sock,stream->text.buf,&stream->text.length) == APR_SUCCESS) {
				status = TRUE;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send MRCPv2 Stream");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate MRCPv2 Stream");
		}
	}
	while(result == MRCP_STREAM_MESSAGE_TRUNCATED);

	return status;
}

static apt_bool_t mrcp_server_message_handler(void *obj, mrcp_message_t *message, mrcp_stream_result_e result)
{
	mrcp_connection_t *connection = obj;
	mrcp_connection_agent_t *agent = connection->agent;
	if(result == MRCP_STREAM_MESSAGE_COMPLETE) {
		/* message is completely parsed */
		mrcp_control_channel_t *channel = mrcp_connection_channel_associate(agent,connection,message);
		if(channel) {
			mrcp_connection_message_receive(agent->vtable,channel,message);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Find Channel <%s@%s>",
				message->channel_id.session_id.buf,
				message->channel_id.resource_name.buf);
		}
	}
	else if(result == MRCP_STREAM_MESSAGE_INVALID) {
		/* error case */
		mrcp_message_t *response;
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse MRCPv2 Stream");
		response = mrcp_response_create(message,message->pool);
		response->start_line.status_code = MRCP_STATUS_CODE_UNRECOGNIZED_MESSAGE;
		if(mrcp_server_agent_messsage_send(agent,connection,response) == FALSE) {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send MRCPv2 Response");
		}
	}
	return TRUE;
}

static apt_bool_t mrcp_server_agent_messsage_receive(mrcp_connection_agent_t *agent, mrcp_connection_t *connection)
{
	apr_status_t status;
	apr_size_t offset;
	apr_size_t length;
	apt_text_stream_t *stream;

	if(!connection || !connection->sock) {
		return FALSE;
	}
	stream = &connection->rx_stream;
	
	/* init length of the stream */
	stream->text.length = sizeof(connection->rx_buffer)-1;
	/* calculate offset remaining from the previous receive / if any */
	offset = stream->pos - stream->text.buf;
	/* calculate available length */
	length = stream->text.length - offset;
	status = apr_socket_recv(connection->sock,stream->pos,&length);
	if(status == APR_EOF || length == 0) {
		return mrcp_server_agent_connection_close(agent,connection);
	}
	/* calculate actual length of the stream */
	stream->text.length = offset + length;
	stream->pos[length] = '\0';
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Receive MRCPv2 Stream [%lu bytes]\n%s",length,stream->pos);

	/* reset pos */
	stream->pos = stream->text.buf;
	/* walk through the stream parsing RTSP messages */
	return mrcp_stream_walk(connection->parser,stream,mrcp_server_message_handler,connection);
}

static apt_bool_t mrcp_server_agent_control_process(mrcp_connection_agent_t *agent)
{
	connection_task_msg_data_t data;
	apr_size_t size = sizeof(connection_task_msg_data_t);
	apr_status_t status = apr_socket_recv(agent->control_sock, (char*)&data, &size);
	if(status == APR_EOF || size == 0) {
		return FALSE;
	}

	switch(data.type) {
		case CONNECTION_TASK_MSG_ADD_CHANNEL:
			mrcp_server_agent_channel_add(agent,data.channel,data.descriptor);
			break;
		case CONNECTION_TASK_MSG_MODIFY_CHANNEL:
			mrcp_server_agent_channel_modify(agent,data.channel,data.descriptor);
			break;
		case CONNECTION_TASK_MSG_REMOVE_CHANNEL:
			mrcp_server_agent_channel_remove(agent,data.channel);
			break;
		case CONNECTION_TASK_MSG_SEND_MESSAGE:
			mrcp_server_agent_messsage_send(agent,data.channel->connection,data.message);
			break;
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
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Start MRCPv2 Agent");
		return FALSE;
	}

	if(mrcp_server_agent_pollset_create(agent) == FALSE) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Pollset");
		return FALSE;
	}

	while(running) {
		status = apr_pollset_poll(agent->pollset, -1, &num, &ret_pfd);
		if(status != APR_SUCCESS) {
			continue;
		}
		for(i = 0; i < num; i++) {
			if(ret_pfd[i].desc.s == agent->listen_sock) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Accept MRCPv2 Connection");
				mrcp_server_agent_connection_accept(agent);
				continue;
			}
			if(ret_pfd[i].desc.s == agent->control_sock) {
				apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process Control Message");
				if(mrcp_server_agent_control_process(agent) == FALSE) {
					running = FALSE;
					break;
				}
				continue;
			}
	
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Process MRCPv2 Message");
			mrcp_server_agent_messsage_receive(agent,ret_pfd[i].client_data);
		}
	}

	mrcp_server_agent_pollset_destroy(agent);

	apt_task_child_terminate(agent->task);
	return TRUE;
}

static apt_bool_t mrcp_server_agent_task_terminate(apt_task_t *task)
{
	apt_bool_t status = FALSE;
	mrcp_connection_agent_t *agent = apt_task_object_get(task);
	if(agent->control_sock) {
		apr_size_t size = sizeof(connection_task_msg_data_t);
		connection_task_msg_data_t data;
		memset(&data,0,size);
		data.type = CONNECTION_TASK_MSG_TERMINATE;
		if(apr_socket_sendto(agent->control_sock,agent->control_sockaddr,0,(const char*)&data,&size) == APR_SUCCESS) {
			status = TRUE;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Send Control Message");
		}
	}
	return status;
}
