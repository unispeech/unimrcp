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

#ifndef APT_MULTIPART_CONTENT_H
#define APT_MULTIPART_CONTENT_H

/**
 * @file apt_multipart_content.h
 * @brief Multipart Content Routine
 */ 

#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** Opaque multipart content declaration */
typedef struct apt_multipart_content_t apt_multipart_content_t;

/**
 * Create an empty multipart content
 * @param max_content_size the max size of the content (body)
 * @param boundary the boundary to separate content parts
 * @param pool the pool to allocate memory from
 * @return an empty multipart content
 */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_create(apr_size_t max_content_size, const apt_str_t *boundary, apr_pool_t *pool);

/** 
 * Add content part to multipart content
 * @param multipart_content the multipart content to add content part to
 * @param content_type the type of content part to add
 * @param content the content part to add
 * @return TRUE on success
 */
APT_DECLARE(apt_bool_t) apt_multipart_content_add(apt_multipart_content_t *multipart_content, const apt_str_t *content_type, const apt_str_t *content);

/** 
 * Finalize multipart content generation 
 * @param multipart_content the multipart content to finalize
 * @return generated multipart content
 */
APT_DECLARE(apt_str_t*) apt_multipart_content_finalize(apt_multipart_content_t *multipart_content);


/** 
 * Assign body to multipart content to get (parse) each content part from
 * @param body the body of multipart content to parse
 * @param boundary the boundary to separate content parts
 * @param pool the pool to allocate memory from
 * @return multipart content with assigned body
 */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_assign(const apt_str_t *body, const apt_str_t *boundary, apr_pool_t *pool);

/** 
 * Get the next content part
 * @param multipart_content the multipart content to get the next content part from
 * @param content_type the type of parsed content part
 * @param content the parsed content part
 * @return TRUE on success
 */
APT_DECLARE(apt_bool_t) apt_multipart_content_get(apt_multipart_content_t *multipart_content, apt_str_t *content_type, apt_str_t *content);


APT_END_EXTERN_C

#endif /* APT_MULTIPART_CONTENT_H */
