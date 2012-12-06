/*
 * Copyright 2008-2010 Arsen Chaloyan
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

#include <windows.h>
#include <apr_getopt.h>
#include "apt.h"
#include "apt_pool.h"

#define WIN_SERVICE_NAME "unimrcp"


/** Display error message with Windows error code and description */
static void winerror(const char *msg)
{
	char buf[128];
	DWORD err = GetLastError();
	int ret = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	printf("%s: %lu %.*s\n", msg, err, ret, buf);
}

/** Register/install service in SCM */
static apt_bool_t uni_service_register(const char *root_dir_path, apr_pool_t *pool)
{
	char *bin_path;
	SERVICE_DESCRIPTION desc;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	bin_path = apr_psprintf(pool,"%s\\bin\\unimrcpserver.exe --service --root-dir \"%s\" -o 2",
					root_dir_path,
					root_dir_path);
	sch_service = CreateService(
					sch_manager,
					WIN_SERVICE_NAME,
					"UniMRCP Server",
					GENERIC_EXECUTE | SERVICE_CHANGE_CONFIG,
					SERVICE_WIN32_OWN_PROCESS,
					SERVICE_DEMAND_START,
					SERVICE_ERROR_NORMAL,
					bin_path,0,0,0,0,0);
	if(!sch_service) {
		winerror("Failed to Create Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	desc.lpDescription = "Launches UniMRCP Server";
	if(!ChangeServiceConfig2(sch_service,SERVICE_CONFIG_DESCRIPTION,&desc)) {
		winerror("Failed to Set Service Description");
	}

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return TRUE;
}

/** Unregister/uninstall service from SCM */
static apt_bool_t uni_service_unregister()
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,DELETE|SERVICE_STOP);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status);
	if(!DeleteService(sch_service)) {
		winerror("Failed to Delete Service");
		status = FALSE;
	}
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Start service */
static apt_bool_t uni_service_start()
{
	apt_bool_t status = TRUE;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,SERVICE_START);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!StartService(sch_service,0,NULL)) {
		winerror("Failed to Start Service");
		status = FALSE;
	}
	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}

/** Stop service */
static apt_bool_t uni_service_stop()
{
	apt_bool_t status = TRUE;
	SERVICE_STATUS ss_status;
	SC_HANDLE sch_service;
	SC_HANDLE sch_manager = OpenSCManager(0,0,SC_MANAGER_ALL_ACCESS);
	if(!sch_manager) {
		winerror("Failed to Open SCManager");
		return FALSE;
	}

	sch_service = OpenService(sch_manager,WIN_SERVICE_NAME,SERVICE_STOP);
	if(!sch_service) {
		winerror("Failed to Open Service");
		CloseServiceHandle(sch_manager);
		return FALSE;
	}

	if(!ControlService(sch_service,SERVICE_CONTROL_STOP,&ss_status)) {
		winerror("Failed to Stop Service");
		status = FALSE;
	}

	CloseServiceHandle(sch_service);
	CloseServiceHandle(sch_manager);
	return status;
}


static void usage()
{
	printf(
		"\n"
		"Usage:\n"
		"\n"
		"  unimrcpservice [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -r [--register] rootdir : Register the Windows service.\n"
		"\n"
		"   -u [--unregister]       : Unregister the Windows service.\n"
		"\n"
		"   -s [--start]            : Start the Windows service.\n"
		"\n"
		"   -t [--stop]             : Stop the Windows service.\n"
		"\n"
		"   -h [--help]             : Show the help.\n"
		"\n");
}

int main(int argc, const char * const *argv)
{
	apr_pool_t *pool;
	apr_status_t rv;
	apr_getopt_t *opt;
	apt_bool_t ret = TRUE;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "register",   'r', TRUE,  "register service" },  /* -r or --register arg */
		{ "unregister", 'u', FALSE, "unregister service" },/* -u or --unregister */
		{ "start",      's', FALSE, "start service" },     /* -s or --start */
		{ "stop",       't', FALSE, "stop service" },      /* -t or --stop */
		{ "help",       'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 1;
	}

	/* create APR pool */
	pool = apt_pool_create();
	if(!pool) {
		apr_terminate();
		return 1;
	}

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv == APR_SUCCESS) {
		int optch;
		const char *optarg;
		while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
			switch(optch) {
				case 'r':
					ret = uni_service_register(optarg,pool);
					break;
				case 'u':
					ret = uni_service_unregister();
					break;
				case 's':
					ret = uni_service_start();
					break;
				case 't':
					ret = uni_service_stop();
					break;
				case 'h':
					usage();
					break;
			}
			if (!ret) break;
		}
		if(rv != APR_EOF) {
			ret = FALSE;
			usage();
		}
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return ret ? 0 : 1;
}
