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

#include "unimrcp_server.h"
#include "mrcp_default_factory.h"
#include "mpf_engine.h"
#include "mrcp_sofiasip_agent.h"
#include "apt_log.h"

static mrcp_sig_agent_t* mrcpv2_sig_agent_create(apr_pool_t *pool);
static mrcp_sig_agent_t* mrcpv1_sig_agent_create(apr_pool_t *pool);

/** Start UniMRCP server */
MRCP_DECLARE(mrcp_server_t*) unimrcp_server_start()
{
	apr_pool_t *pool;
	mrcp_resource_factory_t *resource_factory;
	mpf_engine_t *media_engine;
	mrcp_sig_agent_t *sig_agent;
	mrcp_server_t *server = mrcp_server_create();
	if(!server) {
		return NULL;
	}
	pool = mrcp_server_memory_pool_get(server);

	resource_factory = mrcp_default_factory_create(pool);
	if(resource_factory) {
		mrcp_server_resource_factory_register(server,resource_factory);
	}
	media_engine = mpf_engine_create(pool);
	if(media_engine) {
		mrcp_server_media_engine_register(server,media_engine);
	}
	sig_agent = mrcpv2_sig_agent_create(pool);
	if(sig_agent) {
		mrcp_server_signaling_agent_register(server,sig_agent);
	}

	mrcp_server_start(server);
	return server;
}

/** Shutdown UniMRCP server */
MRCP_DECLARE(apt_bool_t) unimrcp_server_shutdown(mrcp_server_t *server)
{
	if(mrcp_server_shutdown(server) == FALSE) {
		return FALSE;
	}
	return mrcp_server_destroy(server);
}

static mrcp_sig_agent_t* mrcpv2_sig_agent_create(apr_pool_t *pool)
{
	mrcp_sofia_config_t *config = mrcp_sofiasip_config_alloc(pool);
	config->local_ip = "0.0.0.0";
	config->local_port = 8060;
	config->user_agent_name = "UniMRCP Sofia-SIP";
	config->origin = "UniMRCPServer";
	return mrcp_sofiasip_agent_create(config,pool);
}

static mrcp_sig_agent_t* mrcpv1_sig_agent_create(apr_pool_t *pool)
{
	return NULL;
}
