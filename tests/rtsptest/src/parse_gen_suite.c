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
#include "rtsp_message.h"


static apt_bool_t test_file_process(apt_test_suite_t *suite, const char *file_path)
{
	apr_file_t *file;
	char buf_in[1500];
	apt_text_stream_t stream_in;
	rtsp_message_t *message;
	apt_str_t resource_name;

	if(apr_file_open(&file,file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,APR_OS_DEFAULT,suite->pool) != APR_SUCCESS) {
		return FALSE;
	}

	stream_in.text.length = sizeof(buf_in)-1;
	stream_in.text.buf = buf_in;
	stream_in.pos = stream_in.text.buf;
	if(apr_file_read(file,stream_in.text.buf,&stream_in.text.length) != APR_SUCCESS) {
		return FALSE;
	}
	stream_in.text.buf[stream_in.text.length]='\0';

	apt_string_reset(&resource_name);
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s] [%d bytes]\n%s",file_path,stream_in.text.length,stream_in.text.buf);

	do {
		const char *pos = stream_in.pos;
		message = rtsp_message_create(RTSP_MESSAGE_TYPE_UNKNOWN,suite->pool);
		if(rtsp_message_parse(message,&stream_in) == TRUE) {
			char buf_out[1500];
			apt_text_stream_t stream_out;
			apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Parsed Stream [%d bytes]",stream_in.pos - pos);

			stream_out.text.length = sizeof(buf_out)-1;
			stream_out.text.buf = buf_out;
			stream_out.pos = stream_out.text.buf;
			if(rtsp_message_generate(message,&stream_out) == TRUE) {
				*stream_out.pos = '\0';
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Generated Stream [%d bytes]\n%s",stream_out.text.length,stream_out.text.buf);
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate Message");
			}
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Parse Message");
		}
		getchar();
	}
	while(stream_in.pos < stream_in.text.buf + stream_in.text.length);
	apr_file_close(file);

	return TRUE;
}

static apt_bool_t test_dir_process(apt_test_suite_t *suite)
{
	apr_status_t rv;
	apr_dir_t *dir;

	const char *dir_name = "msg";
	if(apr_dir_open(&dir,dir_name,suite->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Cannot Open Directory [%s]",dir_name);
		return FALSE;
	}

	do {
		apr_finfo_t finfo;
		rv = apr_dir_read(&finfo,APR_FINFO_DIRENT,dir);
		if(rv == APR_SUCCESS) {
			if(finfo.filetype == APR_REG && finfo.name) {
				char *file_path;
				apr_filepath_merge(&file_path,dir_name,finfo.name,0,suite->pool);
				test_file_process(suite,file_path);
			}
		}
	} 
	while(rv == APR_SUCCESS);

	apr_dir_close(dir);
	return TRUE;
}

static apt_bool_t parse_gen_test_run(apt_test_suite_t *suite, int argc, const char * const *argv)
{
	test_dir_process(suite);
	return TRUE;
}

apt_test_suite_t* parse_gen_test_suite_create(apr_pool_t *pool)
{
	apt_test_suite_t *suite = apt_test_suite_create(pool,"parse-gen",NULL,parse_gen_test_run);
	return suite;
}
