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

#include <apr_time.h>
#include <apr_file_io.h>
#include "apt_log.h"

#define MAX_LOG_ENTRY_SIZE 4096
#define MAX_PRIORITY_NAME_LENGTH 9

static const char priority_snames[APT_PRIO_COUNT][MAX_PRIORITY_NAME_LENGTH+1] =
{
	"[EMERG]  ",
	"[ALERT]  ",
	"[CRITIC] ",
	"[ERROR]  ",
	"[WARN]   ",
	"[NOTICE] ",
	"[INFO]   ",
	"[DEBUG]  "
};


typedef struct apt_logger_t apt_logger_t;

struct apt_logger_t {
	apt_log_output_e      mode;
	apt_log_priority_e    priority;
	int                   header;
	apt_log_ext_handler_f ext_handler;
	FILE                 *file;
	apr_size_t            cur_size;
	apr_size_t            max_size;
	apr_thread_mutex_t   *mutex;
};

static apt_logger_t apt_logger = {
	APT_LOG_OUTPUT_CONSOLE, 
	APT_PRIO_DEBUG, 
	APT_LOG_HEADER_DEFAULT, 
	NULL,
	NULL,
	0,
	MAX_LOG_FILE_SIZE,
	NULL
};

static apt_bool_t apt_do_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr);


APT_DECLARE(apt_bool_t) apt_log_file_open(const char *file_path, apr_size_t max_size, apr_pool_t *pool)
{
	if(!pool) {
		return FALSE;
	}

	/* create mutex */
	if(apr_thread_mutex_create(&apt_logger.mutex,APR_THREAD_MUTEX_DEFAULT,pool) != APR_SUCCESS) {
		return FALSE;
	}
	/* open log file */
	apt_logger.file = fopen(file_path,"w");
	if(!apt_logger.file) {
		apr_thread_mutex_destroy(apt_logger.mutex);
		return FALSE;
	}
	apt_logger.cur_size = 0;
	apt_logger.max_size = max_size;
	return TRUE;
}

APT_DECLARE(apt_bool_t) apt_log_file_close()
{
	if(apt_logger.file) {
		/* close log file */
		fclose(apt_logger.file);
		apt_logger.file = NULL;
		/* destroy mutex */
		apr_thread_mutex_destroy(apt_logger.mutex);
		apt_logger.mutex = NULL;
	}
	return TRUE;
}

APT_DECLARE(void) apt_log_output_mode_set(apt_log_output_e mode)
{
	apt_logger.mode = mode;
}

APT_DECLARE(void) apt_log_priority_set(apt_log_priority_e priority)
{
	if(priority < APT_PRIO_COUNT) {
		apt_logger.priority = priority;
	}
}

APT_DECLARE(void) apt_log_header_set(int header)
{
	apt_logger.header = header;
}

APT_DECLARE(void) apt_log_ext_handler_set(apt_log_ext_handler_f handler)
{
	apt_logger.ext_handler = handler;
}

APT_DECLARE(apt_bool_t) apt_log(const char *file, int line, apt_log_priority_e priority, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(priority <= apt_logger.priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger.ext_handler) {
			status = apt_logger.ext_handler(file,line,NULL,priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(file,line,priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

static apt_bool_t apt_do_log(const char *file, int line, apt_log_priority_e priority, const char *format, va_list arg_ptr)
{
	char log_entry[MAX_LOG_ENTRY_SIZE];
	apr_size_t offset = 0;
	apr_time_exp_t result;
	apr_time_t now = apr_time_now();
	apr_time_exp_lt(&result,now);

	if(apt_logger.header & APT_LOG_HEADER_DATE) {
		offset += apr_snprintf(log_entry+offset,MAX_LOG_ENTRY_SIZE-offset,"%4d-%02d-%02d ",
							result.tm_year+1900,
							result.tm_mon+1,
							result.tm_mday);
	}
	if(apt_logger.header & APT_LOG_HEADER_TIME) {
		offset += apr_snprintf(log_entry+offset,MAX_LOG_ENTRY_SIZE-offset,"%02d:%02d:%02d:%06d ",
							result.tm_hour,
							result.tm_min,
							result.tm_sec,
							result.tm_usec);
	}
	if(apt_logger.header & APT_LOG_HEADER_MARK) {
		offset += apr_snprintf(log_entry+offset,MAX_LOG_ENTRY_SIZE-offset,"%s:%03d ",file,line);
	}
	if(apt_logger.header & APT_LOG_HEADER_PRIORITY) {
		memcpy(log_entry+offset,priority_snames[priority],MAX_PRIORITY_NAME_LENGTH);
		offset += MAX_PRIORITY_NAME_LENGTH;
	}

	offset += apr_vsnprintf(log_entry+offset,MAX_LOG_ENTRY_SIZE-offset,format,arg_ptr);
	log_entry[offset++] = '\n';
	log_entry[offset] = '\0';
	if((apt_logger.mode & APT_LOG_OUTPUT_CONSOLE) == APT_LOG_OUTPUT_CONSOLE) {
		printf(log_entry);
	}
	
	if((apt_logger.mode & APT_LOG_OUTPUT_FILE) == APT_LOG_OUTPUT_FILE && apt_logger.file) {
		apr_thread_mutex_lock(apt_logger.mutex);

		apt_logger.cur_size += offset;
		if(apt_logger.cur_size > apt_logger.max_size) {
			/* roll over */
			fseek(apt_logger.file,0,SEEK_SET);
			apt_logger.cur_size = offset;
		}
		/* write to log file */
		fwrite(log_entry,1,offset,apt_logger.file);
		fflush(apt_logger.file);

		apr_thread_mutex_unlock(apt_logger.mutex);
	}
	return TRUE;
}
