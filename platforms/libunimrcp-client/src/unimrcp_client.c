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

#include <apr_xml.h>
#include "unimrcp_client.h"
#include "mrcp_default_factory.h"
#include "mpf_engine.h"
#include "mpf_rtp_termination_factory.h"
#include "mrcp_sofiasip_client_agent.h"
#include "mrcp_client_connection.h"
#include "apt_log.h"

#define DEFAULT_CONF_FILE_PATH    "unimrcpclient.xml"
#define DEFAULT_LOCAL_IP_ADDRESS  "127.0.0.1"
#define DEFAULT_REMOTE_IP_ADDRESS "127.0.0.1"

static apr_xml_doc* unimrcp_client_config_parse(const char *path, apr_pool_t *pool);
static apt_bool_t unimrcp_client_config_load(mrcp_client_t *client, const apr_xml_doc *doc, apr_pool_t *pool);

/** Start UniMRCP client */
MRCP_DECLARE(mrcp_client_t*) unimrcp_client_create(const char *conf_file_path)
{
	apr_pool_t *pool;
	apr_xml_doc *doc;
	mrcp_resource_factory_t *resource_factory;
	mrcp_client_t *client = mrcp_client_create();
	if(!client) {
		return NULL;
	}
	pool = mrcp_client_memory_pool_get(client);

	resource_factory = mrcp_default_factory_create(pool);
	if(resource_factory) {
		mrcp_client_resource_factory_register(client,resource_factory);
	}

	doc = unimrcp_client_config_parse(conf_file_path,pool);
	if(doc) {
		unimrcp_client_config_load(client,doc,pool);
	}

	return client;
}

/** Parse config file */
static apr_xml_doc* unimrcp_client_config_parse(const char *path, apr_pool_t *pool)
{
	apr_xml_parser *parser;
	apr_xml_doc *doc;
	apr_file_t *fd;
	apr_status_t rv;

	if(!path) {
		path = DEFAULT_CONF_FILE_PATH;
	}
    
	apt_log(APT_PRIO_INFO,"Loading Config File [%s]",path);
	rv = apr_file_open(&fd,path,APR_READ|APR_BINARY,0,pool);
	if(rv != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Open Config File [%s]",path);
		return NULL;
	}

	rv = apr_xml_parse_file(pool,&parser,&doc,fd,2000);
	if(rv != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Failed to Parse Config File [%s]",path);
		return NULL;
	}

	apr_file_close(fd);
	return doc;
}

static apt_bool_t param_name_value_get(const apr_xml_elem *elem, const apr_xml_attr **name, const apr_xml_attr **value)
{
	const apr_xml_attr *attr;
	if(!name || !value) {
		return FALSE;
	}

	*name = NULL;
	*value = NULL;
	for(attr = elem->attr; attr; attr = attr->next) {
		if(strcasecmp(attr->name,"name") == 0) {
			*name = attr;
		}
		else if(strcasecmp(attr->name,"value") == 0) {
			*value = attr;
		}
	}
	return (*name && *value) ? TRUE : FALSE;
}

/** Load SofiaSIP signaling agent */
static mrcp_sig_agent_t* unimrcp_client_sofiasip_agent_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	mrcp_sofia_client_config_t *config = mrcp_sofiasip_client_config_alloc(pool);
	config->local_ip = DEFAULT_LOCAL_IP_ADDRESS;
	config->local_port = 8062;
	config->remote_ip = DEFAULT_REMOTE_IP_ADDRESS;
	config->remote_port = 8060;
	config->user_agent_name = "UniMRCP Sofia-SIP";
	config->origin = "UniMRCPClient";

	apt_log(APT_PRIO_INFO,"Loading SofiaSIP Agent");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"client-ip") == 0) {
					config->local_ip = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"client-port") == 0) {
					config->local_port = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"server-ip") == 0) {
					config->remote_ip = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"server-port") == 0) {
					config->remote_port = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"ua-name") == 0) {
					config->user_agent_name = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"sdp-origin") == 0) {
					config->origin = apr_pstrdup(pool,attr_value->value);
				}
				else {
					apt_log(APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
	}    
	return mrcp_sofiasip_client_agent_create(config,pool);
}

/** Load UniRTSP signaling agent */
static mrcp_sig_agent_t* unimrcp_client_rtsp_agent_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	return NULL;
}

/** Load signaling agents */
static apt_bool_t unimrcp_client_signaling_agents_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_DEBUG,"Loading Signaling Agents");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"agent") == 0) {
			mrcp_sig_agent_t *sig_agent = NULL;
			const apr_xml_attr *attr;
			for(attr = elem->attr; attr; attr = attr->next) {
				if(strcasecmp(attr->name,"class") == 0) {
					if(strcasecmp(attr->value,"SofiaSIP") == 0) {
						sig_agent = unimrcp_client_sofiasip_agent_load(client,elem,pool);
					}
					else if(strcasecmp(attr->value,"UniRTSP") == 0) {
						sig_agent = unimrcp_client_rtsp_agent_load(client,elem,pool);
					}
					break;
				}
				else {
					apt_log(APT_PRIO_WARNING,"Unknown Attribute <%s>",attr->name);
				}
			}
			if(sig_agent) {
				mrcp_client_signaling_agent_register(client,sig_agent);
			}
		}
		else {
			apt_log(APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}


/** Load MRCPv2 conection agents */
static apt_bool_t unimrcp_client_connection_agents_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_DEBUG,"Loading Connection Agents");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"agent") == 0) {
			mrcp_connection_agent_t *connection_agent;
			apt_log(APT_PRIO_INFO,"Loading Connection Agent");
			connection_agent = mrcp_client_connection_agent_create(pool);
			if(connection_agent) {
				mrcp_client_connection_agent_register(client,connection_agent);
			}
		}
		else {
			apt_log(APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load RTP termination factory */
static mpf_termination_factory_t* unimrcp_client_rtp_factory_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	char *rtp_ip = DEFAULT_LOCAL_IP_ADDRESS;
	apr_port_t rtp_port_min = 4000;
	apr_port_t rtp_port_max = 5000;
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_INFO,"Loading RTP Termination Factory");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"param") == 0) {
			const apr_xml_attr *attr_name;
			const apr_xml_attr *attr_value;
			if(param_name_value_get(elem,&attr_name,&attr_value) == TRUE) {
				apt_log(APT_PRIO_DEBUG,"Loading Param %s:%s",attr_name->value,attr_value->value);
				if(strcasecmp(attr_name->value,"rtp-ip") == 0) {
					rtp_ip = apr_pstrdup(pool,attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtp-port-min") == 0) {
					rtp_port_min = (apr_port_t)atol(attr_value->value);
				}
				else if(strcasecmp(attr_name->value,"rtp-port-max") == 0) {
					rtp_port_max = (apr_port_t)atol(attr_value->value);
				}
				else {
					apt_log(APT_PRIO_WARNING,"Unknown Attribute <%s>",attr_name->value);
				}
			}
		}
	}    
	return mpf_rtp_termination_factory_create(rtp_ip,rtp_port_min,rtp_port_max,pool);
}

/** Load media engines */
static apt_bool_t unimrcp_client_media_engines_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_DEBUG,"Loading Media Engines");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"engine") == 0) {
			mpf_engine_t *media_engine;
			apt_log(APT_PRIO_INFO,"Loading Media Engine");
			media_engine = mpf_engine_create(pool);
			if(media_engine) {
				mrcp_client_media_engine_register(client,media_engine);
			}
		}
		else if(strcasecmp(elem->name,"rtp") == 0) {
			mpf_termination_factory_t *rtp_factory;
			rtp_factory = unimrcp_client_rtp_factory_load(client,elem,pool);
			if(rtp_factory) {
				mrcp_client_rtp_termination_factory_register(client,rtp_factory);
			}
		}
		else {
			apt_log(APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load settings */
static apt_bool_t unimrcp_client_settings_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_DEBUG,"Loading Settings");
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"signaling") == 0) {
			unimrcp_client_signaling_agents_load(client,elem,pool);
		}
		else if(strcasecmp(elem->name,"connection") == 0) {
			unimrcp_client_connection_agents_load(client,elem,pool);
		}
		else if(strcasecmp(elem->name,"media") == 0) {
			unimrcp_client_media_engines_load(client,elem,pool);
		}
		else {
			apt_log(APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}    
	return TRUE;
}

/** Load profiles */
static apt_bool_t unimrcp_client_profiles_load(mrcp_client_t *client, const apr_xml_elem *root, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	apt_log(APT_PRIO_DEBUG,"Loading Profiles");
	for(elem = root->first_child; elem; elem = elem->next) {
		apt_log(APT_PRIO_INFO,"Loading <%s>",elem->name);
	}
    
	return TRUE;
}

/** Load configuration (settings and profiles) */
static apt_bool_t unimrcp_client_config_load(mrcp_client_t *client, const apr_xml_doc *doc, apr_pool_t *pool)
{
	const apr_xml_elem *elem;
	const apr_xml_elem *root = doc->root;
	if(!root || strcasecmp(root->name,"unimrcpclient") != 0) {
		apt_log(APT_PRIO_WARNING,"Unknown Document to Load");
		return FALSE;
	}
	for(elem = root->first_child; elem; elem = elem->next) {
		if(strcasecmp(elem->name,"settings") == 0) {
			unimrcp_client_settings_load(client,elem,pool);
		}
		else if(strcasecmp(elem->name,"profiles") == 0) {
			unimrcp_client_profiles_load(client,elem,pool);
		}
		else {
			apt_log(APT_PRIO_WARNING,"Unknown Element <%s>",elem->name);
		}
	}
    
	return TRUE;
}
