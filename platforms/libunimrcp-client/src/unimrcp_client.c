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

#include "unimrcp_client.h"
#include "mrcp_default_factory.h"
#include "mpf_engine.h"
#include "mpf_rtp_termination_factory.h"
#include "mrcp_sofiasip_agent.h"
#include "mrcp_client_connection.h"
#include "apt_log.h"

static mrcp_sig_agent_t* mrcpv2_sig_agent_create(apr_pool_t *pool);
static mrcp_sig_agent_t* mrcpv1_sig_agent_create(apr_pool_t *pool);

/** Start UniMRCP client */
MRCP_DECLARE(mrcp_client_t*) unimrcp_client_start()
{
	apr_pool_t *pool;
	mrcp_resource_factory_t *resource_factory;
	mpf_engine_t *media_engine;
	mrcp_sig_agent_t *sig_agent;
	mrcp_connection_agent_t *connection_agent;
	mrcp_client_t *client = mrcp_client_create();
	if(!client) {
		return NULL;
	}
	pool = mrcp_client_memory_pool_get(client);

	resource_factory = mrcp_default_factory_create(pool);
	if(resource_factory) {
		mrcp_client_resource_factory_register(client,resource_factory);
	}
	media_engine = mpf_engine_create(pool);
	if(media_engine) {
		mpf_termination_factory_t *rtp_termination_factory = mpf_rtp_termination_factory_create(
			"127.0.0.1",4000,5000,pool);

		mrcp_client_media_engine_register(client,media_engine);
		mrcp_client_rtp_termination_factory_register(client,rtp_termination_factory);
	}
	sig_agent = mrcpv2_sig_agent_create(pool);
	if(sig_agent) {
		mrcp_client_signaling_agent_register(client,sig_agent);
	}
	sig_agent = mrcpv1_sig_agent_create(pool);
	if(sig_agent) {
		mrcp_client_signaling_agent_register(client,sig_agent);
	}
	connection_agent = mrcpv2_client_connection_agent_create(pool);
	if(connection_agent) {
		mrcp_client_connection_agent_register(client,connection_agent);
	}

	mrcp_client_start(client);
	return client;
}

/** Shutdown UniMRCP client */
MRCP_DECLARE(apt_bool_t) unimrcp_client_shutdown(mrcp_client_t *client)
{
	if(mrcp_client_shutdown(client) == FALSE) {
		return FALSE;
	}
	return mrcp_client_destroy(client);
}

static mrcp_sig_agent_t* mrcpv2_sig_agent_create(apr_pool_t *pool)
{
	mrcp_sofia_config_t *config = mrcp_sofiasip_config_alloc(pool);
	config->local_ip = "0.0.0.0";
	config->local_port = 8062;
	config->user_agent_name = "UniMRCP Sofia-SIP";
	config->origin = "UniMRCPClient";
	return mrcp_sofiasip_agent_create(config,pool);
}

static mrcp_sig_agent_t* mrcpv1_sig_agent_create(apr_pool_t *pool)
{
	return NULL;
}
