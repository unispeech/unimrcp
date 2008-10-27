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
#include <stdlib.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include "unimrcp_server.h"
#include "apt_log.h"

static apt_bool_t cmdline_process(char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);

	if(strcasecmp(name,"loglevel") == 0) {
		char *priority = apr_strtok(NULL, " ", &last);
		if(priority) {
			apt_log_priority_set(atol(priority));
		}
	}
	else if(strcasecmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcasecmp(name,"help") == 0) {
		printf("usage:\n");
		printf("- loglevel [level] (set loglevel, one of 0,1...7)\n");
		printf("- quit, exit\n");
	}
	else {
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

static apt_bool_t cmdline_run()
{
	apt_bool_t running = TRUE;
	char cmdline[1024];
	int i;
	do {
		printf(">");
		memset(&cmdline, 0, sizeof(cmdline));
		for(i = 0; i < sizeof(cmdline); i++) {
			cmdline[i] = (char) getchar();
			if(cmdline[i] == '\n') {
				cmdline[i] = '\0';
				break;
			}
		}
		if(*cmdline) {
			running = cmdline_process(cmdline);
		}
	}
	while(running != 0);
	return TRUE;
}

#ifdef WIN32
#include <windows.h>
static void service_cmd_process(int optch)
{
	SC_HANDLE schManager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!schManager) {
		return;
	}

	switch(optch) {
		case 'r':
		{
			SC_HANDLE schService;
			char binPath[MAX_PATH]; 
			if(!GetModuleFileName(NULL,binPath,MAX_PATH)) {
				return;
			}
			schService = CreateService(
							schManager,
							"unimrcp",
							"UniMRCP Server",
							GENERIC_EXECUTE,
							SERVICE_WIN32_OWN_PROCESS,
							SERVICE_DEMAND_START,
							SERVICE_ERROR_NORMAL,
							binPath,0,0,0,0,0);
			if(schService) {
				CloseServiceHandle(schService);
			}
			break;
		}
		case 'u':
		{
			SC_HANDLE schService = OpenService(schManager,"unimrcp",DELETE|SERVICE_STOP);
			if(schService) {
				ControlService(schService,SERVICE_CONTROL_STOP,0);
				DeleteService(schService);
				CloseServiceHandle(schService);
			}
			break;
		}
		case 's':
			break;
		case 'o':
			break;
		default:
			break;
	}
	CloseServiceHandle(schManager);
}
#endif

static void usage()
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  unimrcpserver [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -c [--conf-dir] path   : Set the path to config directory.\n"
		"\n"
		"   -p [--plugin-dir] path : Set the path to plugin directory.\n"
		"\n"
		"   -l [--log] priority    : Set the log priority (0-emergency, ..., 7-debug).\n"
		"\n"
#ifdef WIN32
		"   -r [--register]        : Register the Windows service.\n"
		"\n"
		"   -u [--unregister]      : Unregister the Windows service.\n"
		"\n"
		"   -s [--start]           : Start the Windows service.\n"
		"\n"
		"   -o [--stop]            : Stop the Windows service.\n"
		"\n"
#endif
		"   -h [--help]            : Show the help.\n"
		"\n");
}

static apt_bool_t options_load(const char **conf_dir_path, const char **plugin_dir_path, int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t *opt;
	int optch;
	const char *optarg;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "conf-dir",    'c', TRUE,  "path to config dir" },/* -c arg or --conf-dir arg */
		{ "plugin-dir",  'p', TRUE,  "path to plugin dir" },/* -p arg or --plugin-dir arg */
		{ "log",         'l', TRUE,  "log priority" },      /* -l arg or --log arg */
#ifdef WIN32
		{ "register",    'r', FALSE, "register service" },  /* -r or --register */
		{ "unregister",  'u', FALSE, "unregister service" },/* -u or --unregister */
		{ "start",       's', FALSE, "start service" },     /* -s or --start */
		{ "stop",        'o', FALSE, "stop service" },      /* -o or --stop */
#endif
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* set the default log level */
	apt_log_priority_set(APT_PRIO_INFO);

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		return FALSE;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'c':
				if(conf_dir_path) {
					*conf_dir_path = optarg;
				}
				break;
			case 'p':
				if(plugin_dir_path) {
					*plugin_dir_path = optarg;
				}
				break;
			case 'l':
				if(optarg) {
					apt_log_priority_set(atoi(optarg));
				}
				break;
#ifdef WIN32
			case 'r':
			case 'u':
			case 's':
			case 'o':
				service_cmd_process(optch);
				return FALSE;
#endif
			case 'h':
				usage();
				return FALSE;
		}
	}

	if(rv != APR_EOF) {
		usage();
		return FALSE;
	}

	return TRUE;
}

int main(int argc, const char * const *argv)
{
	apr_pool_t *pool;
	const char *conf_dir_path = NULL;
	const char *plugin_dir_path = NULL;
	mrcp_server_t *server;
	
	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* create APR pool */
	if(apr_pool_create(&pool,NULL) != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* load options */
	if(options_load(&conf_dir_path,&plugin_dir_path,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* start server */
	server = unimrcp_server_start(conf_dir_path, plugin_dir_path);
	if(server) {
		/* run command line */
		cmdline_run();
		/* shutdown server */
		unimrcp_server_shutdown(server);
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	
	/* APR global termination */
	apr_terminate();
	return 0;
}
