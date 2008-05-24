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
#include "apt_log.h"

/** Start UniMRCP server */
MRCP_DECLARE(mrcp_server_t*) unimrcp_server_start()
{
	apr_pool_t *pool;
	mrcp_resource_factory_t *resource_factory;
	mpf_engine_t *media_engine;
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
		apt_task_t *media_task = mpf_task_get(media_engine);
		mrcp_server_media_engine_register(server,media_task);
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
