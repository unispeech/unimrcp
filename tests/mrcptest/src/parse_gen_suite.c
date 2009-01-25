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
#include "mrcp_stream.h"

static apt_bool_t test_stream_generate(apt_test_suite_t *suite, mrcp_generator_t *generator, mrcp_message_t *message)
{
	char buffer[500];
	apt_text_stream_t stream;
	mrcp_stream_result_e result;
	apt_bool_t continuation;

	mrcp_generator_message_set(generator,message);
	do {
		apt_text_stream_init(&stream,buffer,sizeof(buffer)-1);
		continuation = FALSE;
		result = mrcp_generator_run(generator,&stream);
		if(result == MRCP_STREAM_MESSAGE_COMPLETE) {
			stream.text.length = stream.pos - stream.text.buf;
			*stream.pos = '\0';
			apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Generated Stream [%d bytes]\n%s",stream.text.length,stream.text.buf);
		}
		else if(result == MRCP_STREAM_MESSAGE_TRUNCATED) {
			*stream.pos = '\0';
			apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Partially Generated Stream [%d bytes]\n%s",stream.text.length,stream.text.buf);
			continuation = TRUE;
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Generate Message");
		}
	}
	while(continuation == TRUE);
	return TRUE;
}

static apt_bool_t resource_name_read(apr_file_t *file, mrcp_parser_t *parser)
{
	char buffer[100];
	apt_text_stream_t stream;
	apt_bool_t status = FALSE;
	apt_text_stream_init(&stream,buffer,sizeof(buffer)-1);
	if(apr_file_read(file,stream.pos,&stream.text.length) != APR_SUCCESS) {
		return FALSE;
	}

	/* skip the first line in a test file, which indicates resource name */
	if(*stream.pos =='/' && *(stream.pos+1)=='/') {
		apt_str_t line;
		stream.pos += 2;
		if(apt_text_line_read(&stream,&line) == TRUE) {
			apr_off_t offset = stream.pos - stream.text.buf;
			apr_file_seek(file,APR_SET,&offset);
			mrcp_parser_resource_name_set(parser,&line);
			status = TRUE;
		}
	}
	return status;
}

static apt_bool_t test_file_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version, const char *file_path)
{
	apr_file_t *file;
	char buffer[500];
	apt_text_stream_t stream;
	mrcp_parser_t *parser;
	mrcp_generator_t *generator;
	apr_size_t read_length;
	apr_size_t read_offset;
	apt_str_t resource_name;

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Open File [%s]",file_path);
	if(apr_file_open(&file,file_path,APR_FOPEN_READ | APR_FOPEN_BINARY,APR_OS_DEFAULT,suite->pool) != APR_SUCCESS) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Open File");
		return FALSE;
	}

	parser = mrcp_parser_create(factory,suite->pool);
	generator = mrcp_generator_create(factory,suite->pool);

	apt_string_reset(&resource_name);
	if(version == MRCP_VERSION_1) {
		resource_name_read(file,parser);
	}

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
			mrcp_stream_result_e result = mrcp_parser_run(parser,&stream);
			if(result == MRCP_STREAM_MESSAGE_COMPLETE) {
				mrcp_message_t *message;
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Message Parsed [%d bytes]",stream.pos - pos);
				message = mrcp_parser_message_get(parser);
				if(message) {
					test_stream_generate(suite,generator,message);
				}
			}
			else if(result == MRCP_STREAM_MESSAGE_TRUNCATED) {
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

static apt_bool_t test_dir_process(apt_test_suite_t *suite, mrcp_resource_factory_t *factory, mrcp_version_e version)
{
	apr_status_t rv;
	apr_dir_t *dir;

	const char *dir_name = "v2";
	if(version == MRCP_VERSION_1) {
		dir_name = "v1";
	}
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
				test_file_process(suite,factory,version,file_path);
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
	mrcp_resource_factory_t *factory = mrcp_default_factory_create(suite->pool);
	if(!factory) {
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Resource Factory");
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
