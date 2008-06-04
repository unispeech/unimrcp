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
#include "mrcpv2_server_agent.h"

struct mrcpv2_server_agent_t {
	apr_pool_t           *pool;

	apr_pollset_t        *pollset;

	apr_sockaddr_t       *sockaddr;
	apr_socket_t         *listen_sock; /* listening socket */
	apr_pollfd_t          listen_sock_pfd;
};



/** Create connection agent. */
APT_DECLARE(mrcpv2_server_agent_t*) mrcpv2_server_agent_create(
										void *obj,
										const char *listen_ip, 
										apr_port_t listen_port, 
										apr_pool_t *pool)
{
	return NULL;
}

/** Destroy connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_server_agent_destroy(mrcpv2_server_agent_t *agent)
{
	return TRUE;
}

/** Start connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_server_agent_start(mrcpv2_server_agent_t *agent)
{
	return TRUE;
}

/** Terminate connection agent. */
APT_DECLARE(apt_bool_t) mrcpv2_server_agent_terminate(mrcpv2_server_agent_t *agent)
{
	return TRUE;
}
