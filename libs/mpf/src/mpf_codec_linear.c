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

#include "mpf_codec.h"

static mpf_codec_vtable_t l16_vtable;

static mpf_codec_descriptor_t l16_descriptor = {
	11,
	"L16",
	8000,
	1,
	NULL
};

mpf_codec_t* mpf_codec_l16_create(apr_pool_t *pool)
{
	return mpf_codec_create(&l16_vtable,&l16_descriptor,pool);
}
