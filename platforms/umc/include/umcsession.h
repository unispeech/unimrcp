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

#ifndef __UMC_SESSION_H__
#define __UMC_SESSION_H__

/**
 * @file umcsession.h
 * @brief UMC Session
 */ 

#include "mrcp_application.h"

class UmcScenario;

class UmcSession
{
public:
/* ============================ CREATORS =================================== */
	UmcSession(const UmcScenario* pScenario);
	virtual ~UmcSession();

/* ============================ MANIPULATORS =============================== */
	virtual bool Run(const char* pProfileName) = 0;
	virtual bool Terminate();

	void SetMrcpApplication(mrcp_application_t* pMrcpApplication);
	void SetId(int id);

/* ============================ HANDLERS =================================== */
	virtual bool OnSessionTerminate(mrcp_sig_status_code_e status);
	virtual bool OnSessionUpdate(mrcp_sig_status_code_e status);
	virtual bool OnChannelAdd(mrcp_channel_t *channel, mrcp_sig_status_code_e status);
	virtual bool OnChannelRemove(mrcp_channel_t *channel, mrcp_sig_status_code_e status);
	virtual bool OnMessageReceive(mrcp_channel_t *channel, mrcp_message_t *message);
	virtual bool OnTerminateEvent(mrcp_channel_t *channel);
	virtual bool OnResourceDiscover(mrcp_session_descriptor_t* descriptor, mrcp_sig_status_code_e status);

/* ============================ ACCESSORS ================================== */
	const UmcScenario* GetScenario() const;

	int GetId() const;

protected:
/* ============================ MANIPULATORS =============================== */
	bool CreateMrcpSession(const char* pProfileName);
	bool DestroyMrcpSession();

	bool AddMrcpChannel(mrcp_channel_t* pMrcpChannel);
	bool RemoveMrcpChannel(mrcp_channel_t* pMrcpChannel);
	bool SendMrcpRequest(mrcp_channel_t* pMrcpChannel, mrcp_message_t* pMrcpMessage);

	mrcp_channel_t* CreateMrcpChannel(
			mrcp_resource_id resource_id, 
			mpf_termination_t* pTermination, 
			mpf_rtp_termination_descriptor_t* pRtpDescriptor, 
			void* pObj);
	mpf_termination_t* CreateAudioTermination(
			const mpf_audio_stream_vtable_t* pStreamVtable,
			mpf_stream_capabilities_t* pCapabilities,
			void* pObj);
	mrcp_message_t* CreateMrcpMessage(
			mrcp_channel_t* pMrcpChannel, 
			mrcp_method_id method_id);

/* ============================ ACCESSORS ================================== */
	apr_pool_t* GetSessionPool() const;
	const char* GetMrcpSessionId() const;

/* ============================ DATA ======================================= */
	const UmcScenario*  m_pScenario;
	int                 m_Id;

private:
/* ============================ DATA ======================================= */
	mrcp_application_t* m_pMrcpApplication;
	mrcp_session_t*     m_pMrcpSession;
	bool                m_Running;
	bool                m_Terminating;
};


/* ============================ INLINE METHODS ============================= */
inline const UmcScenario* UmcSession::GetScenario() const
{
	return m_pScenario;
}

inline void UmcSession::SetId(int id)
{
	m_Id = id;
}

inline int UmcSession::GetId() const
{
	return m_Id;
}

inline void UmcSession::SetMrcpApplication(mrcp_application_t* pMrcpApplication)
{
	m_pMrcpApplication = pMrcpApplication;
}

#endif /*__UMC_SESSION_H__*/
