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

#include <apr_dso.h>
#include <apr_hash.h>
#include "mrcp_engine_loader.h"

/** Engine loader declaration */
struct mrcp_engine_loader_t {
	/** Table of plugins (apr_dso_handle_t*) */
	apr_hash_t *plugins;
	apr_pool_t *pool;
};


/** Create engine loader */
MRCP_DECLARE(mrcp_engine_loader_t*) mrcp_engine_loader_create(apr_pool_t *pool)
{
	mrcp_engine_loader_t *loader = apr_palloc(pool,sizeof(mrcp_engine_loader_t));
	loader->pool = pool;
	loader->plugins = apr_hash_make(pool);
	return loader;
}

/** Destroy engine loader */
MRCP_DECLARE(apt_bool_t) mrcp_engine_loader_destroy(mrcp_engine_loader_t *loader)
{
	return mrcp_engine_loader_plugins_unload(loader);
}

/** Unload loaded plugins */
MRCP_DECLARE(apt_bool_t) mrcp_engine_loader_plugins_unload(mrcp_engine_loader_t *loader)
{
	apr_hash_index_t *it;
	void *val;
	apr_dso_handle_t *plugin;
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Unload Plugins");
	it=apr_hash_first(loader->pool,loader->plugins);
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		plugin = val;
		if(plugin) {
			apr_dso_unload(plugin);
		}
	}
	apr_hash_clear(loader->plugins);
	return TRUE;
}

/** Load engine plugin */
MRCP_DECLARE(mrcp_resource_engine_t*) mrcp_engine_loader_plugin_load(mrcp_engine_loader_t *loader, const char *path, const char *name)
{
	apt_bool_t status = FALSE;
	apr_dso_handle_t *plugin = NULL;
	apr_dso_handle_sym_t func_handle = NULL;
	mrcp_plugin_creator_f plugin_creator = NULL;
	mrcp_resource_engine_t *engine = NULL;
	if(!path || !name) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load Plugin: invalid params");
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Load Plugin [%s] [%s]",path,name);
	if(apr_dso_load(&plugin,path,loader->pool) == APR_SUCCESS) {
		if(apr_dso_sym(&func_handle,plugin,MRCP_PLUGIN_ENGINE_SYM_NAME) == APR_SUCCESS) {
			if(func_handle) {
				plugin_creator = (mrcp_plugin_creator_f)(intptr_t)func_handle;
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load DSO Symbol: "MRCP_PLUGIN_ENGINE_SYM_NAME);
			apr_dso_unload(plugin);
			return NULL;
		}
		
		if(apr_dso_sym(&func_handle,plugin,MRCP_PLUGIN_LOGGER_SYM_NAME) == APR_SUCCESS) {
			if(func_handle) {
				apt_logger_t *logger = apt_log_instance_get();
				mrcp_plugin_log_accessor_f log_accessor;
				log_accessor = (mrcp_plugin_log_accessor_f)(intptr_t)func_handle;
				log_accessor(logger);
			}
		}
	}
	else {
		char derr[512] = "";
		apr_dso_error(plugin,derr,sizeof(derr));
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Load DSO: %s", derr);
		return NULL;
	}

	if(plugin_creator) {
		engine = plugin_creator(loader->pool);
		if(engine) {
			if(mrcp_plugin_version_check(&engine->plugin_version)) {
				status = TRUE;
				apr_hash_set(loader->plugins,name,APR_HASH_KEY_STRING,plugin);
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Incompatible Plugin Version [%d.%d.%d] < ["PLUGIN_VERSION_STRING"]",
					engine->plugin_version.major,
					engine->plugin_version.minor,
					engine->plugin_version.patch);
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Engine");
		}
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Entry Point Found for Plugin");
	}

	if(status == FALSE) {
		apr_dso_unload(plugin);
	}
	return engine;
}