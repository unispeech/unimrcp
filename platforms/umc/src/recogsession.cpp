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

#include "recogsession.h"
#include "recogscenario.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_recog_header.h"
#include "mrcp_recog_resource.h"

struct RecogChannel
{
	/** MRCP control channel */
	mrcp_channel_t* m_pMrcpChannel;
	/** Streaming is in-progress */
	bool            m_Streaming;
	/** File to read audio stream from */
	FILE*           m_pAudioIn;
	/** Estimated time to complete (used if no audio_in available) */
	apr_size_t      m_TimeToComplete;
};

RecogSession::RecogSession(const RecogScenario* pScenario) :
	UmcSession(pScenario),
	m_pRecogChannel(NULL)
{
}

RecogSession::~RecogSession()
{
}

bool RecogSession::Run(const char* pProfileName)
{
	if(!UmcSession::Run(pProfileName))
		return false;
	
	/* create session */
	CreateMrcpSession(pProfileName);
	
	/* create channel and associate all the required data */
	m_pRecogChannel = CreateRecogChannel();
	if(!m_pRecogChannel) 
	{
		DestroyMrcpSession();
		return false;
	}

	/* add channel to session (send asynchronous request) */
	if(!AddMrcpChannel(m_pRecogChannel->m_pMrcpChannel))
	{
		/* session and channel are still not referenced 
		and both are allocated from session pool and will
		be freed with session destroy call */
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
		DestroyMrcpSession();
		return false;
	}
	return true;
}

bool RecogSession::OnSessionTerminate(mrcp_sig_status_code_e status)
{
	if(m_pRecogChannel)
	{
		FILE* pAudioIn = m_pRecogChannel->m_pAudioIn;
		if(pAudioIn)
		{
			m_pRecogChannel->m_pAudioIn = NULL;
			fclose(pAudioIn);
		}
		
		delete m_pRecogChannel;
		m_pRecogChannel = NULL;
	}
	return UmcSession::OnSessionTerminate(status);
}

static apt_bool_t ReadStream(mpf_audio_stream_t* pStream, mpf_frame_t* pFrame)
{
	RecogChannel* pRecogChannel = (RecogChannel*) pStream->obj;
	if(pRecogChannel && pRecogChannel->m_Streaming) 
	{
		if(pRecogChannel->m_pAudioIn) 
		{
			if(fread(pFrame->codec_frame.buffer,1,pFrame->codec_frame.size,pRecogChannel->m_pAudioIn) == pFrame->codec_frame.size) 
			{
				/* normal read */
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
			}
			else 
			{
				/* file is over */
				pRecogChannel->m_Streaming = false;
			}
		}
		else 
		{
			/* fill with silence in case no file available */
			if(pRecogChannel->m_TimeToComplete >= CODEC_FRAME_TIME_BASE) 
			{
				pFrame->type |= MEDIA_FRAME_TYPE_AUDIO;
				memset(pFrame->codec_frame.buffer,0,pFrame->codec_frame.size);
				pRecogChannel->m_TimeToComplete -= CODEC_FRAME_TIME_BASE;
			}
			else 
			{
				pRecogChannel->m_Streaming = false;
			}
		}
	}
	return TRUE;
}

RecogChannel* RecogSession::CreateRecogChannel()
{
	mrcp_channel_t* pChannel;
	mpf_termination_t* pTermination;
	mpf_stream_capabilities_t* pCapabilities;
	apr_pool_t* pool = GetSessionPool();

	/* create channel */
	RecogChannel *pRecogChannel = new RecogChannel;
	pRecogChannel->m_pMrcpChannel = NULL;
	pRecogChannel->m_Streaming = false;
	pRecogChannel->m_pAudioIn = NULL;
	pRecogChannel->m_TimeToComplete = 0;

	/* create source stream capabilities */
	pCapabilities = mpf_source_stream_capabilities_create(pool);
	GetScenario()->InitCapabilities(pCapabilities);

	static const mpf_audio_stream_vtable_t audio_stream_vtable = 
	{
		NULL,
		NULL,
		NULL,
		ReadStream,
		NULL,
		NULL,
		NULL
	};

	pTermination = CreateAudioTermination(
			&audio_stream_vtable,      /* virtual methods table of audio stream */
			pCapabilities,             /* capabilities of audio stream */
			pRecogChannel);            /* object to associate */

	pChannel = CreateMrcpChannel(
			MRCP_RECOGNIZER_RESOURCE,  /* MRCP resource identifier */
			pTermination,              /* media termination, used to terminate audio stream */
			NULL,                      /* RTP descriptor, used to create RTP termination (NULL by default) */
			pRecogChannel);            /* object to associate */
	if(!pChannel)
	{
		delete pRecogChannel;
		return NULL;
	}
	
	pRecogChannel->m_pMrcpChannel = pChannel;
	return pRecogChannel;
}

bool RecogSession::OnChannelAdd(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelAdd(pMrcpChannel,status))
		return false;

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);

	if(status == MRCP_SIG_STATUS_CODE_SUCCESS) 
	{
		mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_DEFINE_GRAMMAR);
		if(pMrcpMessage)
		{
			GetScenario()->InitDefineGrammarRequest(pMrcpMessage);
			SendMrcpRequest(pRecogChannel->m_pMrcpChannel,pMrcpMessage);
		}
	}
	else 
	{
		/* error case, just terminate the demo */
		Terminate();
	}
	return true;
}

bool RecogSession::OnChannelRemove(mrcp_channel_t* pMrcpChannel, mrcp_sig_status_code_e status)
{
	if(!UmcSession::OnChannelRemove(pMrcpChannel,status))
		return false;

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pRecogChannel)
	{
		FILE* pAudioIn = pRecogChannel->m_pAudioIn;
		if(pAudioIn)
		{
			pRecogChannel->m_pAudioIn = NULL;
			fclose(pAudioIn);
		}
	}
	
	/* terminate the demo */
	return Terminate();
}

bool RecogSession::OnMessageReceive(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage)
{
	if(!UmcSession::OnMessageReceive(pMrcpChannel,pMrcpMessage))
		return false;

	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) 
	{
		/* received MRCP response */
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_DEFINE_GRAMMAR) 
		{
			/* received the response to DEFINE-GRAMMAR request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_COMPLETE) 
			{
				OnDefineGrammar(pMrcpChannel);
			}
			else 
			{
				/* received unexpected response, remove channel */
				RemoveMrcpChannel(pMrcpChannel);
			}
		}
		else if(pMrcpMessage->start_line.method_id == RECOGNIZER_RECOGNIZE)
		{
			/* received the response to RECOGNIZE request */
			if(pMrcpMessage->start_line.request_state == MRCP_REQUEST_STATE_INPROGRESS)
			{
				/* start to stream the speech to recognize */
				if(pRecogChannel) 
				{
					pRecogChannel->m_Streaming = true;
				}
			}
			else 
			{
				/* received unexpected response, remove channel */
				RemoveMrcpChannel(pMrcpChannel);
			}
		}
		else 
		{
			/* received unexpected response */
		}
	}
	else if(pMrcpMessage->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT) 
	{
		if(pMrcpMessage->start_line.method_id == RECOGNIZER_RECOGNITION_COMPLETE) 
		{
			GetScenario()->ParseNLSMLResult(pMrcpMessage);
			if(pRecogChannel) 
			{
				pRecogChannel->m_Streaming = false;
			}
			RemoveMrcpChannel(pMrcpChannel);
		}
		else if(pMrcpMessage->start_line.method_id == RECOGNIZER_START_OF_INPUT) 
		{
			/* received start-of-input, do whatever you need here */
		}
	}
	return true;
}

bool RecogSession::OnDefineGrammar(mrcp_channel_t* pMrcpChannel)
{
	RecogChannel* pRecogChannel = (RecogChannel*) mrcp_application_channel_object_get(pMrcpChannel);
	/* create and send RECOGNIZE request */
	mrcp_message_t* pMrcpMessage = CreateMrcpMessage(pMrcpChannel,RECOGNIZER_RECOGNIZE);
	if(pMrcpMessage)
	{
		GetScenario()->InitRecognizeRequest(pMrcpMessage);
		SendMrcpRequest(pRecogChannel->m_pMrcpChannel,pMrcpMessage);
	}

	if(pRecogChannel)
	{
		const mpf_codec_descriptor_t* pDescriptor = mrcp_application_source_descriptor_get(pMrcpChannel);
		pRecogChannel->m_pAudioIn = GetScenario()->GetAudioIn(pDescriptor,GetMrcpSessionId(),GetSessionPool());
		if(!pRecogChannel->m_pAudioIn)
		{
			/* no audio input availble, set some estimated time to complete instead */
			pRecogChannel->m_TimeToComplete = 5000; // 5 sec
		}
	}
	return TRUE;
}
