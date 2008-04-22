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

#ifndef __APT_LOG_H__
#define __APT_LOG_H__

/**
 * @file apt_log.h
 * @brief Basic Logger
 */ 

#include <stdio.h>
#include <stdarg.h>
#include "apt.h"

APT_BEGIN_EXTERN_C

/** Priority of log messages ordered from highest priority to lowest (rfc3164) */
typedef enum {
	APT_PRIO_EMERGENCY, /** System is unusable */
	APT_PRIO_ALERT,     /** Action must be taken immediately */
	APT_PRIO_CRITICAL,  /** Critical condition */
	APT_PRIO_ERROR,     /** Error condition */
	APT_PRIO_WARNING,   /** Warning condition */
	APT_PRIO_NOTICE,    /** Normal, but significant, condition */
	APT_PRIO_INFO,      /** Informational message */
	APT_PRIO_DEBUG,     /** Debug-level message */

	APT_PRIO_COUNT     	/** Number of priorities */
} apt_log_priority_t;

/** Header (format) of log messages */
typedef enum {
	APT_LOG_HEADER_NONE     = 0x00,
	APT_LOG_HEADER_DATE     = 0x01,
	APT_LOG_HEADER_TIME     = 0x02,
	APT_LOG_HEADER_PRIORITY = 0x04,

	APT_LOG_HEADER_DEFAULT  = APT_LOG_HEADER_DATE | APT_LOG_HEADER_TIME | APT_LOG_HEADER_PRIORITY
} apt_log_priority_t;

/** Prototype of log handler function */
typedef apt_bool_t (*apt_log_handler_f)(apt_log_priority_t priority, const char *format, va_list arg_ptr);

/**
 * Set the logging priority (log level).
 * @param priority the priority to set
 */
APT_DECLARE(void) apt_log_priority_set(apt_log_priority_t priority);

/**
 * Set the header (format) for log messages.
 * @param header the header to set (used as bitmask)
 */
APT_DECLARE(void) apt_log_header_set(int header);

/**
 * Set the external log handler.
 * @param handler the handler to pass log events to
 * @remark default logger is used to output the logs to stdout,
 *         if external log handler isn't set
 */
APT_DECLARE(void) apt_log_handler_set(apt_log_handler_f handler);

/**
 * Do logging.
 * @param priority the priority of the entire log entry
 * @param format the format of the entire log entry
 */
APT_DECLARE(apt_bool_t) apt_log(apt_log_priority_t priority, const char *format, ...);

APT_END_EXTERN_C

#endif /*__APT_LOG_H__*/
