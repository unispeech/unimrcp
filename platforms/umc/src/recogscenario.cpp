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

#include "recogscenario.h"
#include "recogsession.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"
/* NLSML doc include */
#include "apt_nlsml_doc.h"
#include "apt_log.h"

RecogScenario::RecogScenario() :
	UmcScenario("recog"),
	m_ContentType("application/srgs+xml"),
	m_Content(NULL),
	m_SampleRates(MPF_SAMPLE_RATE_8000 | MPF_SAMPLE_RATE_16000),
	m_Codec("LPCM")
{
}

RecogScenario::~RecogScenario()
{
}

bool RecogScenario::Load(apr_pool_t* pool)
{
	/* should be loaded from config file */

	m_Content = LoadFileContent("grammar.xml",pool);
	return true;
}

void RecogScenario::Destroy()
{
}

UmcSession* RecogScenario::CreateSession()
{
	return new RecogSession(this);
}

bool RecogScenario::InitDefineGrammarRequest(mrcp_message_t* pMrcpMessage) const
{
	mrcp_generic_header_t* pGenericHeader;
	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader) 
	{
		/* set generic header fields */
		if(pMrcpMessage->start_line.version == MRCP_VERSION_2) {
			apt_string_assign(&pGenericHeader->content_type,"application/srgs+xml",pMrcpMessage->pool);
		}
		else {
			apt_string_assign(&pGenericHeader->content_type,"application/grammar+xml",pMrcpMessage->pool);
		}
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
		apt_string_assign(&pGenericHeader->content_id,"request1@form-level.store",pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_ID);
	}
	/* set message body */

	if(m_Content)
		apt_string_assign(&pMrcpMessage->body,m_Content,pMrcpMessage->pool);
	return true;
}

bool RecogScenario::InitRecognizeRequest(mrcp_message_t* pMrcpMessage) const
{
	mrcp_generic_header_t* pGenericHeader;
	mrcp_recog_header_t* pRecogHeader;

	/* get/allocate generic header */
	pGenericHeader = (mrcp_generic_header_t*) mrcp_generic_header_prepare(pMrcpMessage);
	if(pGenericHeader)
	{
		/* set generic header fields */
		apt_string_assign(&pGenericHeader->content_type,"text/uri-list",pMrcpMessage->pool);
		mrcp_generic_header_property_add(pMrcpMessage,GENERIC_HEADER_CONTENT_TYPE);
	}
	/* get/allocate recognizer header */
	pRecogHeader = (mrcp_recog_header_t*) mrcp_resource_header_prepare(pMrcpMessage);
	if(pRecogHeader)
	{
		/* set recognizer header fields */
		if(pMrcpMessage->start_line.version == MRCP_VERSION_2)
		{
			pRecogHeader->cancel_if_queue = FALSE;
			mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_CANCEL_IF_QUEUE);
		}
		pRecogHeader->no_input_timeout = 5000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_NO_INPUT_TIMEOUT);
		pRecogHeader->recognition_timeout = 10000;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_RECOGNITION_TIMEOUT);
		pRecogHeader->start_input_timers = TRUE;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_START_INPUT_TIMERS);
		pRecogHeader->confidence_threshold = 0.87f;
		mrcp_resource_header_property_add(pMrcpMessage,RECOGNIZER_HEADER_CONFIDENCE_THRESHOLD);
	}
	/* set message body */
	const char text[] = "session:request1@form-level.store";
	apt_string_assign(&pMrcpMessage->body,text,pMrcpMessage->pool);
	return true;
}

bool RecogScenario::ParseNLSMLResult(mrcp_message_t* pMrcpMessage) const
{
	apr_xml_elem* pInterpret;
	apr_xml_elem* pInstance;
	apr_xml_elem* pInput;
	apr_xml_doc* pDoc = nlsml_doc_load(&pMrcpMessage->body,pMrcpMessage->pool);
	if(!pDoc)
		return false;
	
	/* walk through interpreted results */
	pInterpret = nlsml_first_interpret_get(pDoc);
	for(; pInterpret; pInterpret = nlsml_next_interpret_get(pInterpret)) 
	{
		/* get instance and input */
		nlsml_interpret_results_get(pInterpret,&pInstance,&pInput);
		if(pInstance) 
		{
			/* process instance */
			if(pInstance->first_cdata.first) 
			{
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpreted Instance [%s]",pInstance->first_cdata.first->text);
			}
		}
		if(pInput) 
		{
			/* process input */
			if(pInput->first_cdata.first)
			{
				apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Interpreted Input [%s]",pInput->first_cdata.first->text);
			}
		}
	}
	return true;
}

bool RecogScenario::InitCapabilities(mpf_stream_capabilities_t* pCapabilities) const
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

FILE* RecogScenario::GetAudioIn(const mpf_codec_descriptor_t* pDescriptor, const char* id, apr_pool_t* pool) const
{
	char* pFileName = apr_psprintf(pool,"one-%dkHz.pcm",
		pDescriptor ? pDescriptor->sampling_rate/1000 : 8);
	char* pFilePath = apt_datadir_filepath_get(m_pDirLayout,pFileName,pool);
	if(!pFilePath)
		return NULL;
	
	FILE* pFile = fopen(pFilePath,"rb");
	if(!pFile)
	{
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Cannot Find [%s]",pFilePath);
		return NULL;
	}

	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Set [%s] as Speech Source",pFilePath);
	return pFile;
}
