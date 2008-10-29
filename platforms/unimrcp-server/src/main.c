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

#ifdef WIN32
#include <windows.h>

#define WIN_SERVICE_NAME "unimrcp"

static SERVICE_STATUS_HANDLE win_service_status_handle = NULL;
static SERVICE_STATUS win_service_status;

/** Register/install service in SCM */
static void win_service_register(const char *service_name, apr_pool_t *pool)
{
	char file_path[MAX_PATH];
	char *bin_path;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		apt_log(APT_PRIO_WARNING,"Failed to Open SCManager %d", GetLastError());
		return;
	}

	if(!GetModuleFileName(NULL,file_path,MAX_PATH)) {
		return;
	}
	bin_path = apr_psprintf(pool,"%s --service",file_path);
	sch_service = CreateService(
					sch_manager,
					service_name,
					"UniMRCP Server",
					GENERIC_EXECUTE,
					SERVICE_WIN32_OWN_PROCESS,
					SERVICE_DEMAND_START,
					SERVICE_ERROR_NORMAL,
					bin_path,0,0,0,0,0);
	if(sch_service) {
		CloseServiceHandle(sch_service);
	}
	else {
		apt_log(APT_PRIO_WARNING,"Failed to Create Service %d", GetLastError());
	}
	CloseServiceHandle(sch_manager);
}

/** Unregister/uninstall service from SCM */
static void win_service_unregister(const char *service_name)
{
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		apt_log(APT_PRIO_WARNING,"Failed to Open SCManager %d", GetLastError());
		return;
	}

	sch_service = OpenService(sch_manager,service_name,DELETE|SERVICE_STOP);
	if(sch_service) {
		ControlService(sch_service,SERVICE_CONTROL_STOP,0);
		DeleteService(sch_service);
		CloseServiceHandle(sch_service);
	}
	else {
		apt_log(APT_PRIO_WARNING,"Failed to Open Service %d", GetLastError());
	}
	CloseServiceHandle(sch_manager);
}

/** SCM state change handler */
static void WINAPI win_service_handler(DWORD control)
{
	apt_log(APT_PRIO_INFO,"Service Handler %d",control);
	switch (control)
	{
		case SERVICE_CONTROL_INTERROGATE:
			if(!SetServiceStatus (win_service_status_handle, &win_service_status)) { 
				apt_log(APT_PRIO_WARNING,"Failed to Set Service Status %d",GetLastError());
			} 
			break;
		case SERVICE_CONTROL_STOP:
			win_service_status.dwCurrentState = SERVICE_STOPPED; 
			win_service_status.dwCheckPoint = 0; 
			win_service_status.dwWaitHint = 0; 
			if(!SetServiceStatus (win_service_status_handle, &win_service_status)) { 
				apt_log(APT_PRIO_WARNING,"Failed to Set Service Status %d",GetLastError());
			} 
			break;
	}
}

static void WINAPI win_service_main(DWORD argc, LPTSTR *argv)
{
	apt_log(APT_PRIO_INFO,"Service Main");
	win_service_status_handle = RegisterServiceCtrlHandler(WIN_SERVICE_NAME, win_service_handler);
	if (win_service_status_handle == (SERVICE_STATUS_HANDLE)0) {
		apt_log(APT_PRIO_WARNING,"Failed to Register Service Control Handler %d",GetLastError());
		return;
	} 
	win_service_status.dwServiceType = SERVICE_WIN32; 
	win_service_status.dwCurrentState = SERVICE_RUNNING; 
	win_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP; 
	win_service_status.dwWin32ExitCode = 0; 
	win_service_status.dwServiceSpecificExitCode = 0; 
	win_service_status.dwCheckPoint = 0; 
	win_service_status.dwWaitHint = 0; 
	if(!SetServiceStatus (win_service_status_handle, &win_service_status)) {
		apt_log(APT_PRIO_WARNING,"Failed to Set Service Status %d",GetLastError());
	} 
}

static const SERVICE_TABLE_ENTRY win_service_table[] = {
	{ WIN_SERVICE_NAME, win_service_main },
	{ NULL, NULL }
};

#endif

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
		"   -s [--service]         : Start the Windows service.\n"
		"\n"
#endif
		"   -h [--help]            : Show the help.\n"
		"\n");
}

static apt_bool_t options_load(const char **conf_dir_path, const char **plugin_dir_path, apt_bool_t *service_mode,
							   int argc, const char * const *argv, apr_pool_t *pool)
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
		{ "service",     's', FALSE, "start as service" },  /* -s or --service */
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
				win_service_register(WIN_SERVICE_NAME,pool);
				return FALSE;
			case 'u':
				win_service_unregister(WIN_SERVICE_NAME);
				return FALSE;
			case 's':
				if(service_mode) {
					*service_mode = TRUE;
				}
				break;
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
	apt_bool_t service_mode = FALSE;
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
	if(options_load(&conf_dir_path,&plugin_dir_path,&service_mode,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* start server */
	server = unimrcp_server_start(conf_dir_path, plugin_dir_path);
	if(server) {
		if(service_mode == FALSE) {
			/* run command line */
			cmdline_run();
		}
#ifdef WIN32
		else {
			/* run as windows service */
			apt_log(APT_PRIO_INFO,"Run as Service");
			if(!StartServiceCtrlDispatcher(win_service_table)) {
				/* This is a common error.  Usually, it means the user has
				 invoked the service with the --service flag directly.  This
				 is incorrect.  The only time the --service flag is passed is
				 when the process is being started by the SCM. */
				apt_log(APT_PRIO_WARNING,"Failed to Connect to SCM %d",GetLastError());
			}
		}
#endif
		/* shutdown server */
		unimrcp_server_shutdown(server);
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	
	/* APR global termination */
	apr_terminate();
	return 0;
}
