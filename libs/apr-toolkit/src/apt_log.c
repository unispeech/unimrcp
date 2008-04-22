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
#include "apt_log.h"

#define MAX_LOG_ENTRY_SIZE 1024
#define MAX_PRIORITY_NAME_LENGTH 9

static const char priority_snames[APT_PRIO_COUNT][MAX_PRIORITY_NAME_LENGTH+1] =
{
	"[EMERG]  ",
	"[ALERT]  ",
	"[CRITIC] ",
	"[ERROR]  ",
	"[WARNING]",
	"[NOTICE] ",
	"[INFO]   ",
	"[DEBUG]  "
};


typedef struct apt_logger_t apt_logger_t;

struct apt_logger_t {
	apt_log_priority_t priority;
	int                header;
	apt_log_handler_f  handler;
};

static apt_logger_t apt_logger = {APT_PRIO_DEBUG, APT_LOG_HEADER_DEFAULT, NULL};

static apt_bool_t apt_do_log(apt_log_priority_t priority, const char *format, va_list arg_ptr);


APT_DECLARE(void) apt_log_priority_set(apt_log_priority_t priority)
{
	if(priority < APT_PRIO_COUNT) {
		apt_logger.priority = priority;
	}
}

APT_DECLARE(void) apt_log_header_set(int header)
{
	apt_logger.header = header;
}

APT_DECLARE(void) apt_log_handler_set(apt_log_handler_f handler)
{
	apt_logger.handler = handler;
}

APT_DECLARE(apt_bool_t) apt_log(apt_log_priority_t priority, const char *format, ...)
{
	apt_bool_t status = TRUE;
	if(priority <= apt_logger.priority) {
		va_list arg_ptr;
		va_start(arg_ptr, format);
		if(apt_logger.handler) {
			status = apt_logger.handler(priority,format,arg_ptr);
		}
		else {
			status = apt_do_log(priority,format,arg_ptr);
		}
		va_end(arg_ptr); 
	}
	return status;
}

static apt_bool_t apt_do_log(apt_log_priority_t priority, const char *format, va_list arg_ptr)
{
	char logEntry[MAX_LOG_ENTRY_SIZE];
	apr_size_t offset = 0;
	apr_time_exp_t result;
	apr_time_t now = apr_time_now();
	apr_time_exp_lt(&result,now);

	if(apt_logger.header & APT_LOG_HEADER_DATE) {
		offset += apr_snprintf(logEntry+offset,MAX_LOG_ENTRY_SIZE-offset,"%4ld-%02ld-%02ld ",
							result.tm_year+1900,
							result.tm_mon+1,
							result.tm_mday);
	}
	if(apt_logger.header & APT_LOG_HEADER_TIME) {
		offset += apr_snprintf(logEntry+offset,MAX_LOG_ENTRY_SIZE-offset,"%02ld:%02ld:%02ld:%06ld ",
							result.tm_hour,
							result.tm_min,
							result.tm_sec,
							result.tm_usec);
	}
	if(apt_logger.header & APT_LOG_HEADER_PRIORITY) {
		memcpy(logEntry+offset,priority_snames[priority],MAX_PRIORITY_NAME_LENGTH);
		offset += MAX_PRIORITY_NAME_LENGTH;
	}

	offset += apr_vsnprintf(logEntry+offset,MAX_LOG_ENTRY_SIZE-offset,format,arg_ptr);
	logEntry[offset++] = '\n';
	logEntry[offset++] = '\0';
	printf(logEntry);
	return TRUE;
}
