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

#include "synthscenario.h"
#include "synthsession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"


SynthScenario::SynthScenario() :
	UmcScenario("synth"),
	m_ContentType("application/synthesis+ssml"),
#if 0
	m_ContentType("text/plain"),
#endif
	m_Content(NULL),
	m_SampleRates(MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000),
	m_Codec("LPCM")
{
}

SynthScenario::~SynthScenario()
{
}

bool SynthScenario::Load(apr_pool_t* pool)
{
	/* should be loaded from config file */

	m_Content = LoadFileContent("speak.xml",pool);
	return true;
}

void SynthScenario::Destroy()
{
}

UmcSession* SynthScenario::CreateSession()
{
	return new SynthSession(this);
}

bool SynthScenario::InitSpeakRequest(mrcp_message_t* pMrcpMessage) const
{
	mrcp_generic_header_t* pGenericHeader;
	mrcp_synth_header_t* pSynthHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		/* set generic header fields */
		apt_string_assign(&pGenericHeader->content_type,m_ContentType,pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
	}
	/* get/allocate synthesizer header */
	pSynthHeader = (mrcp_synth_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pSynthHeader) 
	{
		/* set synthesizer header fields */
		pSynthHeader->voice_param.age = 28;
		mrcp_resource_header_property_add(pMrcpMessage,SYNTHESIZER_HEADER_VOICE_AGE);
	}
	/* set message body */
	if(m_Content)
		apt_string_assign(&pMrcpMessage->body,m_Content,pMrcpMessage->pool);
	return true;
}

bool SynthScenario::InitCapabilities(mpf_stream_capabilities_t* pCapabilities) const
{
	/* add codec capabilities (Linear PCM) */
	mpf_codec_capabilities_add(
			&pCapabilities->codecs,
			m_SampleRates,
			m_Codec);

#if 0
	/* more capabilities can be added or replaced */
	mpf_codec_capabilities_add(
			&pCapabilities->codecs,
			MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000,
			"PCMU");
#endif

	return true;

}

FILE* SynthScenario::GetAudioOut(const mpf_codec_descriptor_t* pDescriptor, const char* id, apr_pool_t* pool) const
{
	char* pFileName = apr_psprintf(pool,"synth-%dkHz-%s.pcm",
		pDescriptor ? pDescriptor->sampling_rate/1000 : 8, id);
	char* pFilePath = apt_datadir_filepath_get(m_pDirLayout,pFileName,pool);
	if(!pFilePath) 
		return NULL;

	return fopen(pFilePath,"wb");
}
