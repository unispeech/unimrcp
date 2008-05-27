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

typedef struct mrcp_sofia_agent_t mrcp_sofia_agent_t;
#define NUA_MAGIC_T mrcp_sofia_agent_t

typedef struct mrcp_sofia_session_t mrcp_sofia_session_t;
#define NUA_HMAGIC_T mrcp_sofia_session_t

#include <apr_general.h>
#include <sofia-sip/su.h>
#include <sofia-sip/nua.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sdp.h>

#include "mrcp_sofiasip_agent.h"
#include "apt_log.h"

struct mrcp_sofia_agent_t {
	mrcp_sig_agent_t    *sig_agent;

	mrcp_sofia_config_t *config;
	su_root_t           *root;
	nua_t               *nua;
};


static apt_bool_t mrcp_sofia_task_run(apt_task_t *task);
static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task);

/** Create Sofia-SIP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_sofiasip_agent_create(mrcp_sofia_config_t *config, apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	mrcp_sofia_agent_t *sofia_agent;
	mrcp_sig_agent_t *sig_agent = apr_palloc(pool,sizeof(mrcp_sig_agent_t));
	sig_agent->pool = pool;
	sig_agent->obj = NULL;
	sig_agent->task = NULL;

	sofia_agent = apr_palloc(pool,sizeof(mrcp_sofia_agent_t));
	sofia_agent->sig_agent = sig_agent;
	sofia_agent->config = config;
	sofia_agent->root = NULL;
	sofia_agent->nua = NULL;

	apt_task_vtable_reset(&vtable);
	vtable.run = mrcp_sofia_task_run;
	vtable.terminate = mrcp_sofia_task_terminate;
	sig_agent->task = apt_task_create(sofia_agent,&vtable,NULL,pool);

	return sig_agent;
}

/** Allocate Sofia-SIP config */
MRCP_DECLARE(mrcp_sofia_config_t*) mrcp_sofiasip_config_alloc(apr_pool_t *pool)
{
	mrcp_sofia_config_t *config = apr_palloc(pool,sizeof(mrcp_sofia_config_t));
	config->local_ip = NULL;
	config->local_port = 0;
	config->remote_ip = NULL;
	config->remote_port = 0;
	
	config->user_name = NULL;
	config->user_agent_name = NULL;
	return config;
}

/* This callback will be called by SIP stack to process incoming events */
static void mrcp_sofia_event_callback( nua_event_t                  nua_event,
									   int                          status,
									   char const                  *phrase,
									   nua_t                       *nua,
									   mrcp_sofia_agent_t          *sofia_agent,
									   nua_handle_t                *nh,
									   mrcp_sofia_session_t        *sofia_session,
									   sip_t const                 *sip,
									   tagi_t                       tags[])
{
	apt_log(APT_PRIO_INFO,"Recieve SIP Event [%s] Status %d %s\n",nua_event_name(nua_event),status,phrase);

	switch(nua_event) {
		case nua_i_state:
//			mrcp_sofia_on_state_change(sofia_event);
			break;
		case nua_i_options:
//			mrcp_sofia_on_resource_discover(sofia_event);
			break;
		case nua_r_shutdown:
			/* break main loop of sofia thread */
			su_root_break(sofia_agent->root);
			break;
		default: 
			break;
	}
}

static apt_bool_t mrcp_sofia_task_run(apt_task_t *task)
{
	char *sip_bind_url;
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);

	/* Initialize Sofia-SIP library and create event loop */
	su_init();
	sofia_agent->root = su_root_create(NULL);

	/* Create a user agent instance. The stack will call the 'event_callback()' 
	 * callback when events such as succesful registration to network, 
	 * an incoming call, etc, occur. 
	 */
	sip_bind_url = apr_psprintf(sofia_agent->sig_agent->pool,"sip:%s:%hu",
		sofia_agent->config->local_ip,sofia_agent->config->local_port);
	sofia_agent->nua = nua_create(
					sofia_agent->root,         /* Event loop */
					mrcp_sofia_event_callback, /* Callback for processing events */
					sofia_agent,               /* Additional data to pass to callback */
					NUTAG_URL(sip_bind_url),   /* Address to bind to */
					TAG_END());                /* Last tag should always finish the sequence */
	if(sofia_agent->nua) {
		nua_set_params(
					sofia_agent->nua,
					NUTAG_AUTOANSWER(0),
					NUTAG_APPL_METHOD("OPTIONS"),
					SIPTAG_USER_AGENT_STR(sofia_agent->config->user_agent_name),
					TAG_END());

		/* Run event loop */
		su_root_run(sofia_agent->root);
		
		/* Destroy allocated resources */
		nua_destroy(sofia_agent->nua);
		sofia_agent->nua = NULL;
	}
	su_root_destroy(sofia_agent->root);
	sofia_agent->root = NULL;
	su_deinit();

	apt_task_child_terminate(task);
	return TRUE;
}

static apt_bool_t mrcp_sofia_task_terminate(apt_task_t *task)
{
	mrcp_sofia_agent_t *sofia_agent = apt_task_object_get(task);
	if(sofia_agent->nua) {
		apt_log(APT_PRIO_DEBUG,"Send Shutdown Signal to NUA\n");
		nua_shutdown(sofia_agent->nua);
	}
	return TRUE;
}
