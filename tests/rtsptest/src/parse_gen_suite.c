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

static apt_bool_t test_stream_generate(apt_test_suite_t *suite, rtsp_message_t *message)
{
	char buffer[1500];
	apt_text_stream_t stream;

	stream.text.length = sizeof(buffer)-1;
	stream.text.buf = buffer;
	stream.pos = stream.text.buf;
	if(rtsp_message_generate(message,&stream) == TRUE) {
		*stream.pos = '\0';
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Generated Stream [%d bytes]\n%s",stream.text.length,stream.text.buf);
	}
	else {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate Message");
	}
	return TRUE;
}

static apt_bool_t test_file_process(apt_test_suite_t *suite, const char *file_path)
{
	apr_file_t *file;
	char buffer[500];
	apt_text_stream_t stream;
	rtsp_parser_t *parser;
	apr_size_t read_length;
	apr_size_t read_offset;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s]",file_path);
	if(apr_file_open(&file,file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,APR_OS_DEFAULT,suite->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File");
		return FALSE;
	}

	parser = rtsp_parser_create(suite->pool);

	stream.text.buf = buffer;
	read_offset = 0;
	do {
		stream.pos = stream.text.buf;
		stream.text.length = sizeof(buffer)-1;
		read_length = stream.text.length - read_offset;
		if(apr_file_read(file,stream.pos,&read_length) != APR_SUCCESS) {
			break;
		}
		read_offset = 0;
		stream.text.length = read_offset + read_length;
		stream.text.buf[stream.text.length]='\0';
		apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Stream to Parse [%d bytes]\n%s",stream.text.length,stream.text.buf);

		do {
			const char *pos = stream.pos;
			rtsp_stream_result_e result = rtsp_parser_run(parser,&stream);
			if(result == RTSP_STREAM_MESSAGE_COMPLETE) {
				rtsp_message_t *message;
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Message Parsed [%d bytes]",stream.pos - pos);
				message = rtsp_parser_message_get(parser);
				if(message) {
					test_stream_generate(suite,message);
				}
			}
			else if(result == RTSP_STREAM_MESSAGE_TRUNCATED) {
				apr_size_t scroll_length = stream.pos - stream.text.buf;
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Message Truncated [%d bytes]",stream.pos - pos);
				if(scroll_length && scroll_length != stream.text.length) {
					apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Scroll Stream [%d bytes]",scroll_length);
					memmove(stream.text.buf,stream.pos,scroll_length);
					read_offset = stream.text.length - scroll_length;
				}
				break;
			}
			else {
				apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Invalid Message [%d bytes]",stream.pos - pos);
			}
		}
		while(stream.pos < stream.text.buf + stream.text.length);
	}
	while(apr_file_eof(file) != APR_EOF);

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
				printf("\nPress ENTER to continue\n");
				getchar();
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
