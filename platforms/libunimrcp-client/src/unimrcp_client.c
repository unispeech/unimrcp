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

/** Start UniMRCP client */
MRCP_DECLARE(mrcp_client_t*) unimrcp_client_start()
{
	return NULL;
}

/** Shutdown UniMRCP client */
MRCP_DECLARE(apt_bool_t) unimrcp_client_shutdown(mrcp_client_t *client)
{
	if(mrcp_client_shutdown(client) == FALSE) {
		return FALSE;
	}
	return mrcp_client_destroy(client);
}
