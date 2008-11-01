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

#include <stdlib.h>
#include <apr_getopt.h>
#include "apt_log.h"

#ifdef WIN32
apt_bool_t uni_service_register(apr_pool_t *pool);
apt_bool_t uni_service_unregister();

apt_bool_t uni_service_run(const char *conf_dir_path, const char *plugin_dir_path, apr_pool_t *pool);
#else
apt_bool_t uni_daemon_run(const char *conf_dir_path, const char *plugin_dir_path, apr_pool_t *pool);
#endif

apt_bool_t uni_cmdline_run(const char *conf_dir_path, const char *plugin_dir_path, apr_pool_t *pool);


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
		"   -s [--service]         : Run as the Windows service.\n"
		"\n"
#else
		"   -d [--daemon]          : Run as the daemon.\n"
		"\n"
#endif
		"   -h [--help]            : Show the help.\n"
		"\n");
}

static apt_bool_t options_load(const char **conf_dir_path, const char **plugin_dir_path, apt_bool_t *foreground,
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
		{ "service",     's', FALSE, "run as service" },    /* -s or --service */
#else
		{ "daemon",      'd', FALSE, "start as daemon" },   /* -d or --daemon */
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
				uni_service_register(pool);
				return FALSE;
			case 'u':
				uni_service_unregister();
				return FALSE;
			case 's':
				if(foreground) {
					*foreground = FALSE;
				}
				break;
#else
			case 'd':
				if(foreground) {
					*foreground = FALSE;
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
	apt_bool_t foreground = TRUE;

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
	if(options_load(&conf_dir_path,&plugin_dir_path,&foreground,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	if(foreground == TRUE) {
		/* run command line */
		uni_cmdline_run(conf_dir_path,plugin_dir_path,pool);
	}
#ifdef WIN32
	else {
		/* run as windows service */
		uni_service_run(conf_dir_path,plugin_dir_path,pool);
	}
#else
	else {
		/* run as daemon */
		uni_daemon_run(conf_dir_path,plugin_dir_path,pool);
	}
#endif

	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return 0;
}
