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
#include "demo_framework.h"
#include "apt_log.h"

static apt_bool_t demo_framework_cmdline_process(demo_framework_t *framework, const char *cmdline)
{
	apt_bool_t running = TRUE;
	const char* name = cmdline;
	char* arg;
	if((arg = strchr(cmdline, ' ')) != 0) {
		*arg++ = '\0';
	}

	if(strcmp(name,"run") == 0) {
		char *app_name = arg;
		if(app_name) {
			demo_framework_app_run(framework,app_name);
		}
	}
	else if(strcmp(name,"loglevel") == 0) {
		if(arg) {
			apt_log_priority_set(atol(arg));
		}
	}
	else if(strcmp(name,"exit") == 0 || strcmp(name,"quit") == 0) {
		running = FALSE;
	}
	else if(strcmp(name,"help") == 0) {
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

int main(int argc, const char * const *argv)
{
	demo_framework_t *framework;

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		apr_terminate();
		return 0;
	}

	/* set log level */
	apt_log_priority_set(APT_PRIO_INFO);

	/* create demo framework */
	framework = demo_framework_create();
	if(framework) {
		/* run command line  */
		demo_framework_cmdline_run(framework);
		/* destroy demo framework */
		demo_framework_destroy(framework);
	}
	
	/* APR global termination */
	apr_terminate();
	return 0;
}
