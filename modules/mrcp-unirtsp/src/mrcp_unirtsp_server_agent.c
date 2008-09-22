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

#include <apr_general.h>
//#include <unirtsp-sip/sdp.h>

#include "mrcp_unirtsp_server_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "apt_log.h"

typedef struct mrcp_unirtsp_agent_t mrcp_unirtsp_agent_t;
typedef struct mrcp_unirtsp_session_t mrcp_unirtsp_session_t;

struct mrcp_unirtsp_agent_t {
	mrcp_sig_agent_t     *sig_agent;

	rtsp_server_config_t *config;
};

struct mrcp_unirtsp_session_t {
	mrcp_session_t *session;
};


static apt_bool_t mrcp_unirtsp_task_run(apt_task_t *task);
static apt_bool_t mrcp_unirtsp_task_terminate(apt_task_t *task);

static apt_bool_t mrcp_unirtsp_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_unirtsp_on_session_terminate(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_unirtsp_on_session_answer,
	mrcp_unirtsp_on_session_terminate
};

static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_server_config_t *config, apr_pool_t *pool);


/** Create UniRTSP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_unirtsp_server_agent_create(rtsp_server_config_t *config, apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_unirtsp_agent_t *agent;
	agent = apr_palloc(pool,sizeof(mrcp_unirtsp_agent_t));
	agent->sig_agent = mrcp_signaling_agent_create(agent,MRCP_VERSION_1,pool);
	agent->config = config;

	if(rtsp_config_validate(agent,config,pool) == FALSE) {
		return NULL;
	}

	apt_task_vtable_reset(&vtable);
	vtable.run = mrcp_unirtsp_task_run;
	vtable.terminate = mrcp_unirtsp_task_terminate;
	agent->sig_agent->task = apt_task_create(agent,&vtable,NULL,pool);
	apt_log(APT_PRIO_NOTICE,"Create UniRTSP Agent %s:%hu",config->local_ip,config->local_port);
	return agent->sig_agent;
}

/** Allocate UniRTSP config */
MRCP_DECLARE(rtsp_server_config_t*) mrcp_unirtsp_server_config_alloc(apr_pool_t *pool)
{
	rtsp_server_config_t *config = apr_palloc(pool,sizeof(rtsp_server_config_t));
	config->local_ip = NULL;
	config->local_port = 0;
	config->origin = NULL;
	return config;
}

static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_server_config_t *config, apr_pool_t *pool)
{
	agent->config = config;
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_task_run(apt_task_t *task)
{

	apt_task_child_terminate(task);
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_task_terminate(apt_task_t *task)
{
	return TRUE;
}
#if 0
static mrcp_unirtsp_session_t* mrcp_unirtsp_session_create(mrcp_unirtsp_agent_t *agent)
{
	mrcp_unirtsp_session_t *unirtsp_session;
	mrcp_session_t* session = agent->sig_agent->create_server_session(agent->sig_agent);
	if(!session) {
		return NULL;
	}
	session->response_vtable = &session_response_vtable;
	session->event_vtable = NULL;

	unirtsp_session = apr_palloc(session->pool,sizeof(mrcp_unirtsp_session_t));
	unirtsp_session->session = session;
	session->obj = unirtsp_session;
	
	return unirtsp_session;
}
#endif
static apt_bool_t mrcp_unirtsp_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_session_terminate(mrcp_session_t *session)
{
	return TRUE;
}
