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

#ifndef __MRCP_DEFAULT_FACTORY_H__
#define __MRCP_DEFAULT_FACTORY_H__

/**
 * @file mrcp_default_factory.h
 * @brief Default MRCP Resource Factory
 */ 

#include "mrcp_resource_factory.h"

APT_BEGIN_EXTERN_C

/** Enumeration of MRCP resource types */
typedef enum {
	MRCP_SYNTHESIZER_RESOURCE, /**< Synthesizer resource */
	MRCP_RECOGNIZER_RESOURCE,  /**< Recognizer resource */

	MRCP_RESOURCE_TYPE_COUNT   /**< Number of resources */
} mrcp_resource_type_e;

/** Create default MRCP resource factory */
MRCP_DECLARE(mrcp_resource_factory_t*) mrcp_default_factory_create(apr_pool_t *pool);


APT_END_EXTERN_C

#endif /*__MRCP_DEFAULT_FACTORY_H__*/
