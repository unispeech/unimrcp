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

#include "apt_text_stream.h"

/* Navigate through the lines of the text stream (message) */
APT_DECLARE(apt_bool_t) apt_text_line_read(apt_text_stream_t *stream, apt_str_t *line)
{
	line->length = 0;
	line->buf = stream->pos;
	while(stream->pos < stream->text.buf + stream->text.length) {
		if(*stream->pos == APT_TOKEN_CR) {
			line->length = stream->pos - line->buf;
			stream->pos++;
			if(stream->pos < stream->text.buf + stream->text.length && *stream->pos == APT_TOKEN_LF) {
				stream->pos++;
			}
			break;
		}
		else if(*stream->pos == APT_TOKEN_LF) {
			line->length = stream->pos - line->buf;
			stream->pos++;
			break;
		}
		stream->pos++;
	}
	return TRUE;
}

/* Navigate through the fields of the line */
APT_DECLARE(apt_bool_t) apt_text_field_read(apt_text_stream_t *stream, char separator, apt_bool_t skip_spaces, apt_str_t *field)
{
	char *pos = stream->pos;
	if(skip_spaces == TRUE) {
		while(pos < stream->text.buf + stream->text.length && *pos == APT_TOKEN_SP) {
			pos++;
		}
	}

	field->buf = pos;
	field->length = 0;
	while(pos < stream->text.buf + stream->text.length) {
		if(*pos == separator) {
			field->length = pos - field->buf;
			pos++;
			break;
		}
		pos++;
	}
	stream->pos = pos;
	return TRUE;
}


/** Parse name-value pair */
APT_DECLARE(apt_bool_t) apt_name_value_parse(apt_text_stream_t *stream, apt_name_value_t *pair)
{
	apt_text_field_read(stream,':',TRUE,&pair->name);
	while(stream->pos < stream->text.buf + stream->text.length && *stream->pos == APT_TOKEN_SP) {
		stream->pos++;
	}
	pair->value.buf = stream->pos;
	pair->value.length = stream->text.length - (stream->pos - stream->text.buf);
	return TRUE;
}

/** Generate name-value pair */
APT_DECLARE(apt_bool_t) apt_name_value_generate(const apt_name_value_t *pair, apt_text_stream_t *stream)
{
	return apt_name_and_value_generate(&pair->name,&pair->value,stream);
}

/** Generate name-value pair */
APT_DECLARE(apt_bool_t) apt_name_and_value_generate(const apt_str_t *name, const apt_str_t *value, apt_text_stream_t *stream)
{
	char *pos = stream->pos;
	memcpy(pos,name->buf,name->length);
	pos += name->length;
	*pos = ':';
	pos++;
	memcpy(pos,value->buf,value->length);
	pos += value->length;
	stream->pos = pos;
	return TRUE;
}

/** Generate only the name part ("name:") of the name-value pair */
APT_DECLARE(apt_bool_t) apt_name_value_name_generate(const apt_str_t *name, apt_text_stream_t *stream)
{
	char *pos = stream->pos;
	memcpy(pos,name->buf,name->length);
	pos += name->length;
	*pos = ':';
	pos++;
	stream->pos = pos;
	return TRUE;
}


#define TOKEN_TRUE  "true"
#define TOKEN_FALSE "false"
#define TOKEN_TRUE_LENGTH  (sizeof(TOKEN_TRUE)-1)
#define TOKEN_FALSE_LENGTH (sizeof(TOKEN_FALSE)-1)

/** Parse boolean-value */
APT_DECLARE(apt_bool_t) apt_boolean_value_parse(const char *str, apt_bool_t *value)
{
	if(strncasecmp(str,TOKEN_TRUE,TOKEN_TRUE_LENGTH) == 0) {
		*value = TRUE;
		return TRUE;
	}
	if(strncasecmp(str,TOKEN_FALSE,TOKEN_FALSE_LENGTH) == 0) {
		*value = FALSE;
		return TRUE;
	}
	return FALSE;
}

/** Generate boolean-value */
APT_DECLARE(apt_bool_t) apt_boolean_value_generate(apt_bool_t value, apt_text_stream_t *stream)
{
	if(value == TRUE) {
		memcpy(stream->pos,TOKEN_TRUE,TOKEN_TRUE_LENGTH);
		stream->pos += TOKEN_TRUE_LENGTH;
	}
	else {
		memcpy(stream->pos,TOKEN_FALSE,TOKEN_FALSE_LENGTH);
		stream->pos += TOKEN_FALSE_LENGTH;
	}
	return TRUE;
}


/** Generate value plus the length (number of digits) of the value itself. */
APT_DECLARE(apt_bool_t) apt_var_length_value_generate(apr_size_t *value, apr_size_t max_count, apt_str_t *str)
{
	/* (N >= (10^M-M)) ? N+M+1 : N+M */
	apr_size_t temp;
	apr_size_t count; /* M */
	apr_size_t bounds; /* 10^M */
	int length;

	/* calculate count */
	temp = *value;
	count = 0;
	do{count++; temp /= 10;} while(temp);

	/* calculate bounds */
	temp = count;
	bounds = 1;
	do{bounds *= 10; temp--;} while(temp);

	if(*value >= bounds - count) {
		count++;
	}

	*value += count;
	if(count > max_count) {
		return FALSE;
	}

	str->length = 0;
	length = sprintf(str->buf, "%"APR_SIZE_T_FMT, *value);
	if(length <= 0) {
		return FALSE;
	}
	str->length = length;
	return TRUE;
}
