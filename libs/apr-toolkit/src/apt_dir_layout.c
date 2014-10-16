/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id$
 */

#include <apr_file_info.h>
#include <apr_xml.h>
#include "apt_dir_layout.h"

static apt_dir_layout_t* apt_dir_layout_alloc(apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = (apt_dir_layout_t*) apr_palloc(pool,sizeof(apt_dir_layout_t));
	dir_layout->conf_dir_path = NULL;
	dir_layout->plugin_dir_path = NULL;
	dir_layout->log_dir_path = NULL;
	dir_layout->data_dir_path = NULL;
	dir_layout->var_dir_path = NULL;
	return dir_layout;
}

static const char* apt_default_root_dir_path_get(apr_pool_t *pool)
{
	char *root_dir_path;
	char *cur_dir_path;
	/* Get the current directory */
	if(apr_filepath_get(&cur_dir_path,0,pool) != APR_SUCCESS)
		return NULL;

	/* Root directory is supposed to be one level up by default */
	if(apr_filepath_merge(&root_dir_path,cur_dir_path,"../",0,pool) != APR_SUCCESS)
		return FALSE;

	return root_dir_path;
}

APT_DECLARE(apt_dir_layout_t*) apt_default_dir_layout_create(const char *root_dir_path, apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_alloc(pool);

	if(!root_dir_path) {
		/* If root dir path is not specified, get the default one */
		root_dir_path = apt_default_root_dir_path_get(pool);
	}
	
	if(root_dir_path) {
		apr_filepath_merge(&dir_layout->conf_dir_path,root_dir_path,"conf",0,pool);
		apr_filepath_merge(&dir_layout->plugin_dir_path,root_dir_path,"plugin",0,pool);
		apr_filepath_merge(&dir_layout->log_dir_path,root_dir_path,"log",0,pool);
		apr_filepath_merge(&dir_layout->data_dir_path,root_dir_path,"data",0,pool);
		apr_filepath_merge(&dir_layout->var_dir_path,root_dir_path,"var",0,pool);
	}
	return dir_layout;
}

APT_DECLARE(apt_dir_layout_t*) apt_custom_dir_layout_create(
									const char *conf_dir_path,
									const char *plugin_dir_path,
									const char *log_dir_path,
									const char *data_dir_path,
									const char *var_dir_path,
									apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout = apt_dir_layout_alloc(pool);
	if(conf_dir_path) {
		dir_layout->conf_dir_path = apr_pstrdup(pool,conf_dir_path);
	}
	if(plugin_dir_path) {
		dir_layout->plugin_dir_path = apr_pstrdup(pool,plugin_dir_path);
	}
	if(log_dir_path) {
		dir_layout->log_dir_path = apr_pstrdup(pool,log_dir_path);
	}
	if(data_dir_path) {
		dir_layout->data_dir_path = apr_pstrdup(pool,data_dir_path);
	}
	if(var_dir_path) {
		dir_layout->var_dir_path = apr_pstrdup(pool,var_dir_path);
	}
	return dir_layout;
}

static apr_xml_doc* apt_dir_layout_doc_parse(const char *file_path, apr_pool_t *pool)
{
	apr_xml_parser *parser = NULL;
	apr_xml_doc *xml_doc = NULL;
	apr_file_t *fd = NULL;
	apr_status_t rv;

	rv = apr_file_open(&fd,file_path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&xml_doc,fd,2000);
	if(rv != APR_SUCCESS) {
		xml_doc = NULL;
	}
	
	apr_file_close(fd);
	return xml_doc;
}

static APR_INLINE apr_status_t apt_dir_is_path_absolute(const char *path, apr_pool_t *pool)
{
	const char *root_path;
	const char *file_path = path;
	return apr_filepath_root(&root_path,&file_path,0,pool);
}

static char* apt_dir_layout_subdir_parse(const char *root_dir_path, const apr_xml_elem *elem, apr_pool_t *pool)
{
	char *path;
	char *full_path = NULL;
	apr_status_t status;

	if(!elem || !elem->first_cdata.first || !elem->first_cdata.first->text) {
		return NULL;
	}
	
	path = apr_pstrdup(pool,elem->first_cdata.first->text);
	apr_collapse_spaces(path,path);

	/* Check if path is absolute or relative */
	status = apt_dir_is_path_absolute(path,pool);
	if(status == APR_SUCCESS) {
		/* Absolute path specified */
		return path;
	}
	else if (status == APR_ERELATIVE) {
		/* Relative path specified -> merge it with the root path */
		if(apr_filepath_merge(&full_path,root_dir_path,path,APR_FILEPATH_NATIVE,pool) == APR_SUCCESS) {
			return full_path;
		}
	}

	/* WARNING: invalid path specified */
	return NULL;
}

APT_DECLARE(apt_dir_layout_t*) apt_dir_layout_load(const char *config_file, apr_pool_t *pool)
{
	apt_dir_layout_t *dir_layout;
	apr_xml_doc *doc;
	const apr_xml_elem *elem;
	const apr_xml_elem *root;
	const apr_xml_attr *xml_attr;
	char *path;
	const char *root_dir_path = NULL;

	/* Parse XML document */
	doc = apt_dir_layout_doc_parse(config_file,pool);
	if(!doc) {
		return NULL;
	}

	root = doc->root;

	/* Match document name */
	if(!root || strcasecmp(root->name,"dirlayout") != 0) {
		/* Unknown document */
		return NULL;
	}

	dir_layout = apt_dir_layout_alloc(pool);

	/* Find rootdir attribute */
	for(xml_attr = root->attr; xml_attr; xml_attr = xml_attr->next) {
		if(strcasecmp(xml_attr->name, "rootdir") == 0) {
			root_dir_path = xml_attr->value;
			break;
		}
	}

	if(root_dir_path) {
		/* If root dir path is specified, check if it is absolute or relative */
		apr_status_t status = apt_dir_is_path_absolute(root_dir_path,pool);
		if(status == APR_ERELATIVE) {
			/* Relative path specified -> make it absolute */
			char *full_path;
			char *cur_dir_path;
			/* Get the current directory */
			if(apr_filepath_get(&cur_dir_path,APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
				return NULL;

			/* Merge it with path specified */
			if(apr_filepath_merge(&full_path,cur_dir_path,root_dir_path,APR_FILEPATH_NATIVE,pool) != APR_SUCCESS)
				return NULL;
			root_dir_path = full_path;
		}
	}
	else {
		/* If root dir path is not specified, get the default one */
		root_dir_path = apt_default_root_dir_path_get(pool);
	}

	/* Navigate through document */
	for(elem = root->first_child; elem; elem = elem->next) {
		path = apt_dir_layout_subdir_parse(root_dir_path,elem,pool);
		if(!path)
			continue;

		if(strcasecmp(elem->name,"confdir") == 0) {
			dir_layout->conf_dir_path = path;
		}
		else if(strcasecmp(elem->name,"plugindir") == 0) {
			dir_layout->plugin_dir_path = path;
		}
		else if(strcasecmp(elem->name,"logdir") == 0) {
			dir_layout->log_dir_path = path;
		}
		else if(strcasecmp(elem->name,"datadir") == 0) {
			dir_layout->data_dir_path = path;
		}
		else if(strcasecmp(elem->name,"vardir") == 0) {
			dir_layout->var_dir_path = path;
		}
		else {
			/* Unknown element */
		}
	}
	return dir_layout;
}

APT_DECLARE(char*) apt_confdir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	if(dir_layout && dir_layout->conf_dir_path && file_name) {
		char *file_path = NULL;
		if(apr_filepath_merge(&file_path,dir_layout->conf_dir_path,file_name,0,pool) == APR_SUCCESS) {
			return file_path;
		}
	}
	return NULL;
}

APT_DECLARE(char*) apt_datadir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	if(dir_layout && dir_layout->data_dir_path && file_name) {
		char *file_path = NULL;
		if(apr_filepath_merge(&file_path,dir_layout->data_dir_path,file_name,0,pool) == APR_SUCCESS) {
			return file_path;
		}
	}
	return NULL;
}

APT_DECLARE(char*) apt_vardir_filepath_get(const apt_dir_layout_t *dir_layout, const char *file_name, apr_pool_t *pool)
{
	if(dir_layout && dir_layout->var_dir_path && file_name) {
		char *file_path = NULL;
		if(apr_filepath_merge(&file_path,dir_layout->var_dir_path,file_name,0,pool) == APR_SUCCESS) {
			return file_path;
		}
	}
	return NULL;
}
