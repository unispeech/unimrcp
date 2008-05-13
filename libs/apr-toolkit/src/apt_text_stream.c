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
APT_DECLARE(apt_bool_t) apt_text_line_read(apt_text_stream_t *text_stream, apt_str_t *line)
{
	line->length = 0;
	line->buf = text_stream->pos;
	while(text_stream->pos < text_stream->text.buf + text_stream->text.length) {
		if(*text_stream->pos == APT_TOKEN_CR) {
			line->length = text_stream->pos - line->buf;
			text_stream->pos++;
			if(text_stream->pos < text_stream->text.buf + text_stream->text.length && *text_stream->pos == APT_TOKEN_LF) {
				text_stream->pos++;
			}
			break;
		}
		else if(*text_stream->pos == APT_TOKEN_LF) {
			line->length = text_stream->pos - line->buf;
			text_stream->pos++;
			break;
		}
		text_stream->pos++;
	}
	return TRUE;
}

/* Navigate through the fields of the line */
APT_DECLARE(apt_bool_t) apt_text_field_read(apt_str_t *line, char separator, apt_bool_t skip_spaces, apt_str_t *field)
{
	const char *pos = line->buf;
	if(skip_spaces == TRUE) {
		while(pos < line->buf + line->length && *pos == APT_TOKEN_SP) {
			pos++;
		}
	}

	field->buf = pos;
	field->length = 0;
	while(pos < line->buf + line->length)	{
		if(*pos == separator) {
			field->length = pos - field->buf;
			pos++;
			break;
		}
		pos++;
	}
	line->length -= pos - line->buf;
	line->buf = pos;
	return TRUE;
}
