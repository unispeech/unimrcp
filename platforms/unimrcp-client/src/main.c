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
#include "demo_framework.h"
#include "apt_log.h"

static apt_bool_t demo_framework_cmdline_process(demo_framework_t *framework, char *cmdline)
{
	apt_bool_t running = TRUE;
	char *name;
	char *last;
	name = apr_strtok(cmdline, " ", &last);

	if(strcasecmp(name,"run") == 0) {
		char *app_name = apr_strtok(NULL, " ", &last);
		if(app_name) {
			char *profile_name = apr_strtok(NULL, " ", &last);
			if(!profile_name) {
				profile_name = "MRCPv2-Default";
			}
			demo_framework_app_run(framework,app_name,profile_name);
		}
	}
	else if(strcasecmp(name,"loglevel") == 0) {
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
		printf("- run [app_name] (run demo application, app_name is one of 'synth', 'recog')\n");
		printf("- loglevel [level] (set loglevel, one of 0,1...7)\n");
		printf("- quit, exit\n");
	}
	else {
		printf("unknown command: %s (input help for usage)\n",name);
	}
	return running;
}

static apt_bool_t demo_framework_cmdline_run(demo_framework_t *framework)
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
			running = demo_framework_cmdline_process(framework,cmdline);
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
		"  unimrcpclient [options]\n"
		"\n"
		"  Available options:\n"
		"\n"
		"   -f [--conf-file] path : Set the path to configuration file.\n"
		"\n"
		"   -l [--log] priority   : Set the log priority (0-emergency, ..., 7-debug).\n"
		"\n"
		"   -h [--help]           : Show the help.\n"
		"\n");
}

static apt_bool_t demo_framework_options_load(const char **conf_file_path, int argc, const char * const *argv, apr_pool_t *pool)
{
	apr_status_t rv;
	apr_getopt_t *opt;
	int optch;
	const char *optarg;

	static const apr_getopt_option_t opt_option[] = {
		/* long-option, short-option, has-arg flag, description */
		{ "conf-file",    'f', TRUE,  "path to conf file" }, /* -f arg or --conf-file arg */
		{ "log",          'l', TRUE,  "log priority" },      /* -l arg or --log arg */
		{ "help",         'h', FALSE, "show help" },         /* -h or --help */
		{ NULL, 0, 0, NULL },                                /* end */
	};

	/* set the default log level */
	apt_log_priority_set(APT_PRIO_INFO);

	rv = apr_getopt_init(&opt, pool , argc, argv);
	if(rv != APR_SUCCESS) {
		return FALSE;
	}

	while((rv = apr_getopt_long(opt, opt_option, &optch, &optarg)) == APR_SUCCESS) {
		switch(optch) {
			case 'f':
				if(conf_file_path) {
					*conf_file_path = optarg;
				}
				break;
			case 'l':
				if(optarg) {
					apt_log_priority_set(atoi(optarg));
				}
				break;
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
	const char *conf_file_path = NULL;
	demo_framework_t *framework;

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
	if(demo_framework_options_load(&conf_file_path,argc,argv,pool) != TRUE) {
		apr_pool_destroy(pool);
		apr_terminate();
		return 0;
	}

	/* create demo framework */
	framework = demo_framework_create(conf_file_path);
	if(framework) {
		/* run command line  */
		demo_framework_cmdline_run(framework);
		/* destroy demo framework */
		demo_framework_destroy(framework);
	}

	/* create APR pool */
	apr_pool_destroy(pool);
	
	/* APR global termination */
	apr_terminate();
	return 0;
}
