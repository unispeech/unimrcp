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

#include <apr_file_info.h>
#include <apr_file_io.h>
#include "apt_test_suite.h"
#include "apt_log.h"
#include "mrcp_default_factory.h"
#include "mrcp_message.h"


static apt_bool_t test_file_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version, const char *file_path)
{
	apr_file_t *file;
	char buf_in[1500];
	apt_text_stream_t stream;
	mrcp_message_t *message;

	if(apr_file_open(&file,file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,APR_OS_DEFAULT,suite->pool) != APR_SUCCESS) {
		return FALSE;
	}

	stream.text.length = sizeof(buf_in)-1;
	stream.text.buf = buf_in;
	stream.pos = stream.text.buf;
	if(apr_file_read(file,stream.text.buf,&stream.text.length) != APR_SUCCESS) {
		return FALSE;
	}
	stream.text.buf[stream.text.length]='\0';

	message = mrcp_message_create(suite->pool);
	if(version == MRCP_VERSION_1) {
		/* skip the first line in a test file, which indicates resource name */
		if(*stream.pos =='/' && *(stream.pos+1)=='/') {
			stream.pos += 2;
			apt_text_line_read(&stream,&message->channel_id.resource_name);
			stream.text.length -= stream.pos - stream.text.buf;
			stream.text.buf = stream.pos;
		}
	}
	apt_log(APT_PRIO_INFO,"Open File [%s] [%d bytes]\n%s",file_path,stream.text.length,stream.text.buf);

	if(mrcp_message_parse(factory,message,&stream) == TRUE) {
		char buf_out[1500];
		apt_log(APT_PRIO_INFO,"Parsed Stream [%d bytes]",stream.pos - stream.text.buf);

		stream.text.length = sizeof(buf_out)-1;
		stream.text.buf = buf_out;
		stream.pos = stream.text.buf;
		message->start_line.length = 0;
		if(mrcp_message_generate(factory,message,&stream) == TRUE) {
			*stream.pos = '\0';
			apt_log(APT_PRIO_INFO,"Generated Stream [%d bytes]\n%s",stream.text.length,stream.text.buf);
		}
		else {
			apt_log(APT_PRIO_WARNING,"Failed to Generate Message");
		}
	}
	else {
		apt_log(APT_PRIO_WARNING,"Failed to Parse Message");
	}
	apr_file_close(file);

//	getchar();
	return TRUE;
}

static apt_bool_t test_dir_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version)
{
	apr_status_t rv;
	apr_dir_t *dir;

	const char *dir_name = "v2";
	if(version == MRCP_VERSION_1) {
		dir_name = "v1";
	}
	if(apr_dir_open(&dir,dir_name,suite->pool) != APR_SUCCESS) {
		apt_log(APT_PRIO_WARNING,"Cannot Open Directory [%s]",dir_name);
		return FALSE;
	}

	do {
		apr_finfo_t finfo;
		rv = apr_dir_read(&finfo,APR_FINFO_DIRENT,dir);
		if(rv == APR_SUCCESS) {
			if(finfo.filetype == APR_REG && finfo.name) {
				char *file_path;
				apr_filepath_merge(&file_path,dir_name,finfo.name,0,suite->pool);
				test_file_process(suite,factory,version,file_path);
			}
		}
	} 
	while(rv == APR_SUCCESS);

	apr_dir_close(dir);
	return TRUE;
}

static apt_bool_t parse_gen_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	mrcp_resource_factory_t *factory = mrcp_default_factory_create(suite->pool);
	if(!factory) {
		apt_log(APT_PRIO_WARNING,"Failed to Create Resource Factory");
		return FALSE;
	}

	test_dir_process(suite,factory,MRCP_VERSION_1);
	test_dir_process(suite,factory,MRCP_VERSION_2);

	mrcp_resource_factory_destroy(factory);
	return TRUE;
}

apt_test_suite_t* parse_gen_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"parse-gen",NULL,parse_gen_test_run);
	return suite;
}
