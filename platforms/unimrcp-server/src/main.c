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

typedef struct {
	const char        *conf_dir_path;
	const char        *plugin_dir_path;
	apt_bool_t         foreground;
	apt_log_priority_e log_priority;
	apt_log_output_e   log_output;
} server_options_t;

#ifdef WIN32
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
		"   -c [--conf-dir] path     : Set the path to config directory.\n"
		"\n"
		"   -p [--plugin-dir] path   : Set the path to plugin directory.\n"
		"\n"
		"   -l [--log-prio] priority : Set the log priority.\n"
		"                              (0-emergency, ..., 7-debug)\n"
		"\n"
		"   -o [--log-output] mode   : Set the log output mode.\n"
		"                              (0-none, 1-console only, 2-file only, 3-both)\n"
		"\n"
#ifdef WIN32
		"   -s [--service]           : Run as the Windows service.\n"
		"\n"
#else
		"   -d [--daemon]            : Run as the daemon.\n"
		"\n"
#endif
		"   -h [--help]              : Show the help.\n"
		"\n");
}

static apt_bool_t options_load(server_options_t *options, int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t *opt;
	int optch;
	const char *optarg;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "conf-dir",    'c', TRUE,  "path to config dir" },/* -c arg or --conf-dir arg */
		{ "plugin-dir",  'p', TRUE,  "path to plugin dir" },/* -p arg or --plugin-dir arg */
		{ "log-prio",    'l', TRUE,  "log priority" },      /* -l arg or --log-prio arg */
		{ "log-output",  'o', TRUE,  "log output mode" },   /* -o arg or --log-output arg */
#ifdef WIN32
		{ "service",     's', FALSE, "run as service" },    /* -s or --service */
#else
		{ "daemon",      'd', FALSE, "start as daemon" },   /* -d or --daemon */
#endif
		{ "help",        'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                               /* end */
	};

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		return FALSE;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'c':
				options->conf_dir_path = optarg;
				break;
			case 'p':
				options->plugin_dir_path = optarg;
				break;
			case 'l':
				if(optarg) {
					options->log_priority = atoi(optarg);
				}
				break;
			case 'o':
				if(optarg) {
					options->log_output = atoi(optarg);
				}
				break;
#ifdef WIN32
			case 's':
				options->foreground = FALSE;
				break;
#else
			case 'd':
				options->foreground = FALSE;
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
	server_options_t options;

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

	/* set the default options */
	options.conf_dir_path = NULL;
	options.plugin_dir_path = NULL;
	options.foreground = TRUE;
	options.log_priority = APT_PRIO_INFO;
	options.log_output = APT_LOG_OUTPUT_CONSOLE;

	/* load options */
	if(options_load(&options,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* set the log level */
	apt_log_priority_set(options.log_priority);
	/* set the log output mode */
	apt_log_output_mode_set(options.log_output);

	if((options.log_output & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE) {
		/* open the log file */
		apt_log_file_open("unimrcpserver.log");
	}

	if(options.foreground == TRUE) {
		/* run command line */
		uni_cmdline_run(options.conf_dir_path,options.plugin_dir_path,pool);
	}
#ifdef WIN32
	else {
		/* run as windows service */
		uni_service_run(options.conf_dir_path,options.plugin_dir_path,pool);
	}
#else
	else {
		/* run as daemon */
		uni_daemon_run(options.conf_dir_path,options.plugin_dir_path,pool);
	}
#endif

	if((options.log_output & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE) {
		apt_log_file_close();
	}

	/* destroy APR pool */
	apr_pool_destroy(pool);
	/* APR global termination */
	apr_terminate();
	return 0;
}
