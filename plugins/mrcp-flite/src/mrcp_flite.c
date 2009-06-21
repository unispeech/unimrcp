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

#include "mrcp_resource_engine.h"
#include "mrcp_synth_resource.h"
#include "mrcp_synth_header.h"
#include "mrcp_generic_header.h"
#include "mrcp_message.h"
#include "apt_log.h"


/** Declare this macro to use log routine of the server, plugin is loaded from */
MRCP_PLUGIN_LOGGER_IMPLEMENT

/** Create flite engine */
MRCP_PLUGIN_DECLARE(mrcp_resource_engine_t*) mrcp_plugin_create(apr_pool_t *pool)
{
	return NULL;
}
