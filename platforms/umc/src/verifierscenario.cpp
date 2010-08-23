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
#include "verifierscenario.h"
#include "verifiersession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
#include "apt_log.h"

VerifierScenario::VerifierScenario() :
	m_AudioSource(NULL)
{
}

VerifierScenario::~VerifierScenario()
{
}

void VerifierScenario::Destroy()
{
}

bool VerifierScenario::LoadElement(const apr_xml_elem* pElem, apr_pool_t* pool)
{
	if(UmcScenario::LoadElement(pElem,pool))
		return true;
			
	return false;
}


UmcSession* VerifierScenario::CreateSession()
{
	return new VerifierSession(this);
}
