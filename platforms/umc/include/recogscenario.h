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

#ifndef __RECOG_SCENARIO_H__
#define __RECOG_SCENARIO_H__

/**
 * @file recogscenario.h
 * @brief Recognizer Scenario
 */ 

#include "umcscenario.h"

class RecogScenario : public UmcScenario
{
public:
/* ============================ CREATORS =================================== */
	RecogScenario();
	virtual ~RecogScenario();

/* ============================ MANIPULATORS =============================== */
	virtual bool Load(apr_pool_t* pool);
	virtual void Destroy();

	virtual UmcSession* CreateSession();

	bool InitDefineGrammarRequest(mrcp_message_t* pMrcpMessage) const;
	bool InitRecognizeRequest(mrcp_message_t* pMrcpMessage) const;
	bool ParseNLSMLResult(mrcp_message_t* pMrcpMessage) const;
	bool InitCapabilities(mpf_stream_capabilities_t* pCapabilities) const;
	FILE* GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, const char* id, apr_pool_t* pool) const;

protected:
/* ============================ DATA ======================================= */
	const char* m_ContentType;
	const char* m_Content;
	int         m_SampleRates;
	const char* m_Codec;
};

#endif /*__RECOG_SCENARIO_H__*/
