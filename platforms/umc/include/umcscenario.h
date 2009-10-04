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

#ifndef __UMC_SCENARIO_H__
#define __UMC_SCENARIO_H__

/**
 * @file umcscenario.h
 * @brief UMC Scenario
 */ 

#include "mrcp_application.h"

class UmcSession;

class UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	UmcScenario(const char* pName);
	virtual ~UmcScenario();

/* ============================ MANIPULATORS =============================== */
	virtual bool Load(apr_pool_t* pool);
	virtual void Destroy();

	virtual UmcSession* CreateSession() = 0;

	void SetDirLayout(apt_dir_layout_t* pDirLayout);

/* ============================ ACCESSORS ================================== */
	const char* GetName() const {return m_pName;}

protected:
/* ============================ MANIPULATORS =============================== */
	const char* LoadFileContent(const char* pFileName, apr_pool_t* pool);

/* ============================ DATA ======================================= */
	const char*       m_pName;
	apt_dir_layout_t* m_pDirLayout;
};


/* ============================ INLINE METHODS ============================= */
inline void UmcScenario::SetDirLayout(apt_dir_layout_t* pDirLayout)
{
	m_pDirLayout = pDirLayout;
}

#endif /*__UMC_SCENARIO_H__*/
