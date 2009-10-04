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

#include "umcscenario.h"


UmcScenario::UmcScenario(const char* pName) :
	m_pName(pName),
	m_pDirLayout(NULL)
{
}

UmcScenario::~UmcScenario()
{
}

bool UmcScenario::Load(apr_pool_t* pool)
{
	return true;
}

void UmcScenario::Destroy()
{
}

const char* UmcScenario::LoadFileContent(const char* pFileName, apr_pool_t* pool)
{
	if(!m_pDirLayout || !pFileName)
		return NULL;

	char* pFilePath = apt_datadir_filepath_get(m_pDirLayout,pFileName,pool);
	if(!pFilePath)
		return NULL;

	FILE* pFile = fopen(pFilePath,"r");
	if(!pFile)
		return NULL;

	char text[1024];
	apr_size_t size;
	size = fread(text,1,sizeof(text)-1,pFile);
	text[size] = '\0';
	fclose(pFile);
	return apr_pstrdup(pool,text);
}
