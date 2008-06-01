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

#include <stdio.h>
#include "unimrcp_client.h"

int main(int argc, const char * const *argv)
{
	mrcp_client_t *client;
	
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	client = unimrcp_client_start();
	if(client) {
		getchar();
		unimrcp_client_shutdown(client);
	}

	apr_terminate();
	return 0;
}
