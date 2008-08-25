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

#ifndef __UNIMRCP_SERVER_H__
#define __UNIMRCP_SERVER_H__

/**
 * @file unimrcp_server.h
 * @brief UniMRCP Server
 */ 

#include "mrcp_server.h"

APT_BEGIN_EXTERN_C

/** 
 * Start UniMRCP server.
 * @param conf_dir_path the path to config directory
 * @param plugin_dir_path the path to plugin directory
 */
MRCP_DECLARE(mrcp_server_t*) unimrcp_server_start(const char *conf_dir_path, const char *plugin_dir_path);

/** 
 * Shutdown UniMRCP server.
 * @param server the MRCP server to shutdown
 */
MRCP_DECLARE(apt_bool_t) unimrcp_server_shutdown(mrcp_server_t *server);

APT_END_EXTERN_C

#endif /*__UNIMRCP_SERVER_H__*/
