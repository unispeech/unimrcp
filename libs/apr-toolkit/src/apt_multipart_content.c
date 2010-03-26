/*
 * Copyright 2008-2010 Arsen Chaloyan
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
 * 
 * $Id$
 */

#include <stdlib.h>
#include "apt_multipart_content.h"
#include "apt_text_stream.h"

#define CONTENT_LENGTH_HEADER "Content-Length"
#define CONTENT_TYPE_HEADER   "Content-Type"

#define DEFAULT_BOUNDARY      "break"
#define DEFAULT_HYPHENS        "--"

#define DEFAULT_MULTIPART_CONTENT_SIZE 4096

/** Multipart content */
struct apt_multipart_content_t {
	apr_pool_t       *pool;
	apt_text_stream_t stream;

	apt_str_t         boundary;
	apt_str_t         hyphens;
	apt_str_t         content_length_header;
	apt_str_t         content_type_header;
};

static void apt_multipart_content_headers_init(apt_multipart_content_t *multipart_content)
{
	multipart_content->content_length_header.buf = CONTENT_LENGTH_HEADER;
	multipart_content->content_length_header.length = sizeof(CONTENT_LENGTH_HEADER)-1;

	multipart_content->content_type_header.buf = CONTENT_TYPE_HEADER;
	multipart_content->content_type_header.length = sizeof(CONTENT_TYPE_HEADER)-1;
}

/** Create an empty multipart content */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_create(apr_size_t max_content_size, const apt_str_t *boundary, apr_pool_t *pool)
{
	char *buffer;
	apt_multipart_content_t *multipart_content = apr_palloc(pool,sizeof(apt_multipart_content_t));
	multipart_content->pool = pool;

	if(max_content_size == 0) {
		max_content_size = DEFAULT_MULTIPART_CONTENT_SIZE;
	}

	if(boundary) {
		multipart_content->boundary = *boundary;
	}
	else {
		multipart_content->boundary.buf = DEFAULT_BOUNDARY;
		multipart_content->boundary.length = sizeof(DEFAULT_BOUNDARY)-1;
	}

	multipart_content->hyphens.buf = DEFAULT_HYPHENS;
	multipart_content->hyphens.length = sizeof(DEFAULT_HYPHENS)-1;

	apt_multipart_content_headers_init(multipart_content);

	buffer = apr_palloc(pool,max_content_size+1);
	apt_text_stream_init(&multipart_content->stream,buffer,max_content_size);
	return multipart_content;
}

/** Add content part to multipart content */
APT_DECLARE(apt_bool_t) apt_multipart_content_add(apt_multipart_content_t *multipart_content, const apt_str_t *content_type, const apt_str_t *content)
{
	if(!content || !content_type) {
		return FALSE;
	}

	if(multipart_content->hyphens.length + multipart_content->boundary.length + 
		multipart_content->content_type_header.length + content_type->length +
		multipart_content->content_length_header.length + 10 /* max number of digits in content-length */ +
		content->length + 10 /* 5*2 eol */ + multipart_content->stream.pos > multipart_content->stream.end  ) { 
		return FALSE;
	}

	/* insert preceding end-of-line */
	apt_text_eol_insert(&multipart_content->stream);
	/* insert hyphens */
	apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens);
	/* insert boundary */
	apt_text_string_insert(&multipart_content->stream,&multipart_content->boundary);
	apt_text_eol_insert(&multipart_content->stream);

	/* insert content-type */
	apt_text_header_name_insert(&multipart_content->stream,&multipart_content->content_type_header);
	apt_text_string_insert(&multipart_content->stream,content_type);
	apt_text_eol_insert(&multipart_content->stream);

	/* insert content-lebgth */
	apt_text_header_name_insert(&multipart_content->stream,&multipart_content->content_length_header);
	apt_text_size_value_insert(&multipart_content->stream,content->length);
	apt_text_eol_insert(&multipart_content->stream);

	/* insert empty line */
	apt_text_eol_insert(&multipart_content->stream);

	/* insert content */
	apt_text_string_insert(&multipart_content->stream,content);
	return TRUE;
}

/** Finalize multipart content generation */
APT_DECLARE(apt_str_t*) apt_multipart_content_finalize(apt_multipart_content_t *multipart_content)
{
	apt_text_stream_t *stream = &multipart_content->stream;
	if(stream->pos + 4 + 2*multipart_content->hyphens.length + 
		multipart_content->boundary.length > stream->end) {
		return NULL;
	}

	/* insert preceding end-of-line */
	apt_text_eol_insert(&multipart_content->stream);
	/* insert hyphens */
	apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens);
	/* insert boundary */
	apt_text_string_insert(&multipart_content->stream,&multipart_content->boundary);
	/* insert final hyphens */
	apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens);
	apt_text_eol_insert(&multipart_content->stream);

	stream->text.length = stream->pos - stream->text.buf;
	stream->text.buf[stream->text.length] = '\0';
	return &stream->text;
}


/** Assign body to multipart content to get (parse) each content part from */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_assign(const apt_str_t *body, const apt_str_t *boundary, apr_pool_t *pool)
{
	apt_multipart_content_t *multipart_content = apr_palloc(pool,sizeof(apt_multipart_content_t));
	multipart_content->pool = pool;

	if(!body) {
		return FALSE;
	}

	if(boundary) {
		multipart_content->boundary = *boundary;
	}
	else {
		apt_string_reset(&multipart_content->boundary);
	}

	apt_string_reset(&multipart_content->hyphens);
	apt_multipart_content_headers_init(multipart_content);

	apt_text_stream_init(&multipart_content->stream,body->buf,body->length);
	return multipart_content;
}

/** Get the next content part */
APT_DECLARE(apt_bool_t) apt_multipart_content_get(apt_multipart_content_t *multipart_content, apt_str_t *content_type, apt_str_t *content)
{
	apt_str_t boundary;
	apt_pair_t header;
	apt_bool_t is_final = FALSE;
	apt_text_stream_t *stream = &multipart_content->stream;

	if(!content || !content_type) {
		return FALSE;
	}

	apt_string_reset(content);

	/* skip preamble */
	apt_text_skip_to_char(stream,'-');
	if(apt_text_is_eos(stream) == TRUE) {
		return FALSE;
	}

	/* skip initial hyphens */
	apt_text_chars_skip(stream,'-');
	if(apt_text_is_eos(stream) == TRUE) {
		return FALSE;
	}

	/* read line and the boundary */
	if(apt_text_line_read(stream,&boundary) == FALSE) {
		return FALSE;
	}

	/* remove optional trailing spaces */
	while(boundary.length && boundary.buf[boundary.length-1] == APT_TOKEN_SP) boundary.length--;

	/* check whether this is the final boundary */
	if(boundary.length >= 2) {
		if(boundary.buf[boundary.length-1] == '-' && boundary.buf[boundary.length-2] == '-') {
			/* final boundary */
			boundary.length -= 2;
			is_final = TRUE;
		}
	}

	/* compare boundaries */
	if(apt_string_is_empty(&multipart_content->boundary) == TRUE) {
		/* no boundary was specified from user space, 
		learn boundary from the content */
		multipart_content->boundary = boundary;
	}
	else {
		if(apt_string_compare(&multipart_content->boundary,&boundary) == FALSE) {
			/* invalid boundary */
			return FALSE;
		}
	}

	if(is_final == TRUE) {
		/* final boundary => return TRUE, content remains empty */
		return TRUE;
	}

	/* read header fields */
	do {
		if(apt_text_header_read(stream,&header) == TRUE) {
			if(header.name.length) {
				if(apt_string_compare(&multipart_content->content_type_header,&header.name) == TRUE) {
					apt_string_copy(content_type,&header.value,multipart_content->pool);
				}
				else if(apt_string_compare(&multipart_content->content_length_header,&header.name) == TRUE) {
					if(header.value.buf) {
						content->length = atol(header.value.buf);
						if(content->length) {
							content->buf = apr_palloc(multipart_content->pool,content->length+1);
							content->buf[content->length] = '\0';
						}
					}
				}
			}
			else {
				/* empty header => exit */
				break;
			}
		}
	}
	while(apt_text_is_eos(stream) == FALSE);

	if(!content->length || content->length + stream->pos > stream->end) {
		return FALSE;
	}

	/* read content */
	memcpy(content->buf,stream->pos,content->length);
	stream->pos += content->length;
	return TRUE;
}
