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

#include "mpf_context.h"

struct mpf_context_t {
	apr_pool_t *pool;
	void       *obj;
};

struct mpf_termination_t {
	apr_pool_t *pool;
	void       *obj;
};

MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_pool_t *pool)
{
	mpf_context_t *context = apr_palloc(pool,sizeof(mpf_context_t));
	context->obj = obj;
	context->pool = pool;
	return context;
}

MPF_DECLARE(mpf_termination_t*) mpf_termination_create(void *obj, apr_pool_t *pool)
{
	mpf_termination_t *termination = apr_palloc(pool,sizeof(mpf_termination_t));
	termination->obj = obj;
	termination->pool = pool;
	return termination;
}
