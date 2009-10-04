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

#include "umcframework.h"
#include "umcsession.h"
#include "synthscenario.h"
#include "recogscenario.h"
#include "unimrcp_client.h"
#include "apt_log.h"

typedef struct
{
	int                       m_SessionId;
	char                      m_ScenarioName[128];
	char                      m_ProfileName[128];
	const mrcp_app_message_t* m_pAppMessage;
} UmcTaskMsg;

enum UmcTaskMsgType
{
	UMC_TASK_CLIENT_MSG,
	UMC_TASK_RUN_SESSION_MSG,
	UMC_TASK_KILL_SESSION_MSG
};


UmcFramework::UmcFramework() :
	m_pPool(NULL),
	m_pDirLayout(NULL),
	m_pTask(NULL),
	m_pMrcpClient(NULL),
	m_pMrcpApplication(NULL),
	m_CurSessionId(0),
	m_Ready(false),
	m_pScenarioTable(NULL),
	m_pSessionTable(NULL)
{
}

UmcFramework::~UmcFramework()
{
}

bool UmcFramework::Create(apt_dir_layout_t* pDirLayout, apr_pool_t* pool)
{
	m_pDirLayout = pDirLayout;
	m_pPool = pool;

	m_pSessionTable = apr_hash_make(m_pPool);
	m_pScenarioTable = apr_hash_make(m_pPool);
	if(!CreateTask())
		return false;

	/* wait for READY state,
	   preferably cond wait object should be used */
	int attempts = 0;
	while(!m_Ready && attempts < 10)
	{
		attempts++;
		apr_sleep(500000);
	}

	return true;
}

void UmcFramework::Destroy()
{
	DestroyTask();

	m_pScenarioTable = NULL;
	m_pSessionTable = NULL;
}

bool UmcFramework::CreateMrcpClient()
{
	/* create MRCP client stack first */
	m_pMrcpClient = unimrcp_client_create(m_pDirLayout);
	if(!m_pMrcpClient)
		return false;

	/* create MRCP application to send/get requests to/from MRCP client stack */
	m_pMrcpApplication = mrcp_application_create(AppMessageHandler,this,m_pPool);
	if(!m_pMrcpApplication)
	{
		mrcp_client_destroy(m_pMrcpClient);
		m_pMrcpClient = NULL;
		return false;
	}

	/* register MRCP application to MRCP client */
	mrcp_client_application_register(m_pMrcpClient,m_pMrcpApplication,"UMC");
	/* start MRCP client stack processing */
	if(mrcp_client_start(m_pMrcpClient) == FALSE)
	{
		mrcp_client_destroy(m_pMrcpClient);
		m_pMrcpClient = NULL;
		m_pMrcpApplication = NULL;
		return false;
	}
	return true;
}

void UmcFramework::DestroyMrcpClient()
{
	if(m_pMrcpClient)
	{
		/* shutdown MRCP client stack processing first (blocking call) */
		mrcp_client_shutdown(m_pMrcpClient);
		/* destroy MRCP client stack */
		mrcp_client_destroy(m_pMrcpClient);
		m_pMrcpClient = NULL;
		m_pMrcpApplication = NULL;
	}
}

bool UmcFramework::CreateTask()
{
	apt_task_t* pTask;
	apt_task_vtable_t* pVtable;
	apt_task_msg_pool_t* pMsgPool;

	pMsgPool = apt_task_msg_pool_create_dynamic(sizeof(UmcTaskMsg),m_pPool);
	m_pTask = apt_consumer_task_create(this,pMsgPool,m_pPool);
	if(!m_pTask)
		return false;

	pTask = apt_consumer_task_base_get(m_pTask);
	apt_task_name_set(pTask,"Framework Task");
	pVtable = apt_consumer_task_vtable_get(m_pTask);
	if(pVtable) 
	{
		pVtable->process_msg = UmcProcessMsg;
		pVtable->on_start_complete = UmcOnStartComplete;
		pVtable->on_terminate_complete = UmcOnTerminateComplete;
	}

	m_Ready = false;
	apt_task_start(pTask);
	return true;
}

void UmcFramework::DestroyTask()
{
	if(m_pTask) 
	{
		apt_task_t* pTask = apt_consumer_task_base_get(m_pTask);
		if(pTask)
		{
			apt_task_terminate(pTask,TRUE);
			apt_task_destroy(pTask);
		}
		m_pTask = NULL;
	}
}

bool UmcFramework::LoadScenarios()
{
	SynthScenario* pSynthScenario = new SynthScenario();
	pSynthScenario->SetDirLayout(m_pDirLayout);
	pSynthScenario->Load(m_pPool);
	apr_hash_set(m_pScenarioTable,pSynthScenario->GetName(),APR_HASH_KEY_STRING,pSynthScenario);

	RecogScenario* pRecogScenario = new RecogScenario();
	pRecogScenario->SetDirLayout(m_pDirLayout);
	pRecogScenario->Load(m_pPool);
	apr_hash_set(m_pScenarioTable,pRecogScenario->GetName(),APR_HASH_KEY_STRING,pRecogScenario);
	return true;
}

void UmcFramework::DestroyScenarios()
{
	UmcScenario* pScenario;
	void *val;
	apr_hash_index_t *it = apr_hash_first(m_pPool,m_pScenarioTable);
	for(; it; it = apr_hash_next(it)) 
	{
		apr_hash_this(it,NULL,NULL,&val);
		pScenario = (UmcScenario*) val;
		if(pScenario)
		{
			pScenario->Destroy();
			delete pScenario;
		}
	}
	apr_hash_clear(m_pScenarioTable);
}

bool UmcFramework::AddSession(UmcSession* pSession)
{
	if(!pSession)
		return false;
	apr_hash_set(m_pSessionTable,pSession,sizeof(pSession),pSession);
	return true;
}

bool UmcFramework::RemoveSession(UmcSession* pSession)
{
	if(!pSession)
		return false;
	apr_hash_set(m_pSessionTable,pSession,sizeof(pSession),NULL);
	return true;
}

bool UmcFramework::ProcessRunRequest(int id, const char* pScenarioName, const char* pProfileName)
{
	UmcScenario* pScenario = (UmcScenario*) apr_hash_get(m_pScenarioTable,pScenarioName,APR_HASH_KEY_STRING);
	if(!pScenario)
		return false;

	UmcSession* pSession = pScenario->CreateSession();
	if(!pSession)
		return false;

	pSession->SetId(id);
	pSession->SetMrcpApplication(m_pMrcpApplication);
	AddSession(pSession);
	return pSession->Run(pProfileName);
}

void UmcFramework::ProcessKillRequest(int id)
{
	UmcSession* pSession;
	void *val;
	apr_hash_index_t *it = apr_hash_first(m_pPool,m_pSessionTable);
	for(; it; it = apr_hash_next(it)) 
	{
		apr_hash_this(it,NULL,NULL,&val);
		pSession = (UmcSession*) val;
		if(pSession && pSession->GetId() == id)
		{
			/* first, terminate session */
			pSession->Terminate();
			return;
		}
	}
}

int UmcFramework::RunSession(const char* pScenarioName, const char* pProfileName)
{
	apt_task_t* pTask = apt_consumer_task_base_get(m_pTask);
	apt_task_msg_t* pTaskMsg = apt_task_msg_get(pTask);
	if(!pTaskMsg) 
		return 0;

	if(m_CurSessionId == INT_MAX)
		m_CurSessionId = 0;
	m_CurSessionId++;
	
	pTaskMsg->type = TASK_MSG_USER;
	pTaskMsg->sub_type = UMC_TASK_RUN_SESSION_MSG;
	UmcTaskMsg* pUmcMsg = (UmcTaskMsg*) pTaskMsg->data;
	pUmcMsg->m_SessionId = m_CurSessionId;
	strncpy(pUmcMsg->m_ScenarioName,pScenarioName,sizeof(pUmcMsg->m_ScenarioName)-1);
	strncpy(pUmcMsg->m_ProfileName,pProfileName,sizeof(pUmcMsg->m_ProfileName)-1);
	pUmcMsg->m_pAppMessage = NULL;
	apt_task_msg_signal(pTask,pTaskMsg);
	return m_CurSessionId;
}

void UmcFramework::KillSession(int id)
{
	apt_task_t* pTask = apt_consumer_task_base_get(m_pTask);
	apt_task_msg_t* pTaskMsg = apt_task_msg_get(pTask);
	if(!pTaskMsg) 
		return;

	pTaskMsg->type = TASK_MSG_USER;
	pTaskMsg->sub_type = UMC_TASK_KILL_SESSION_MSG;
	
	UmcTaskMsg* pUmcMsg = (UmcTaskMsg*) pTaskMsg->data;
	pUmcMsg->m_SessionId = id;
	pUmcMsg->m_pAppMessage = NULL;
	apt_task_msg_signal(pTask,pTaskMsg);
}

apt_bool_t AppMessageHandler(const mrcp_app_message_t* pMessage)
{
	UmcFramework* pFramework = (UmcFramework*) mrcp_application_object_get(pMessage->application);
	if(!pFramework)
		return FALSE;

	apt_task_t* pTask = apt_consumer_task_base_get(pFramework->m_pTask);
	apt_task_msg_t* pTaskMsg = apt_task_msg_get(pTask);
	if(pTaskMsg) 
	{
		pTaskMsg->type = TASK_MSG_USER;
		pTaskMsg->sub_type = UMC_TASK_CLIENT_MSG;
		
		UmcTaskMsg* pUmcMsg = (UmcTaskMsg*) pTaskMsg->data;
		pUmcMsg->m_pAppMessage = pMessage;
		apt_task_msg_signal(pTask,pTaskMsg);
	}
	
	return TRUE;
}


apt_bool_t OnSessionUpdate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnSessionUpdate(status);
}

apt_bool_t OnSessionTerminate(mrcp_application_t *application, mrcp_session_t *session, mrcp_sig_status_code_e status)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	if(!pSession->OnSessionTerminate(status))
		return false;

	UmcFramework* pFramework = (UmcFramework*) mrcp_application_object_get(application);
	pFramework->RemoveSession(pSession);
	delete pSession;
	return true;
}

apt_bool_t OnChannelAdd(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnChannelAdd(channel,status);
}

apt_bool_t OnChannelRemove(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_sig_status_code_e status)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnChannelRemove(channel,status);
}

apt_bool_t OnMessageReceive(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel, mrcp_message_t *message)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnMessageReceive(channel,message);
}

apt_bool_t OnTerminateEvent(mrcp_application_t *application, mrcp_session_t *session, mrcp_channel_t *channel)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnTerminateEvent(channel);
}

apt_bool_t OnResourceDiscover(mrcp_application_t *application, mrcp_session_t *session, mrcp_session_descriptor_t *descriptor, mrcp_sig_status_code_e status)
{
	UmcSession* pSession = (UmcSession*) mrcp_application_session_object_get(session);
	return pSession->OnResourceDiscover(descriptor,status);
}

apt_bool_t OnReady(mrcp_application_t *application, mrcp_sig_status_code_e status)
{
	UmcFramework* pFramework = (UmcFramework*) mrcp_application_object_get(application);
	pFramework->m_Ready = true;
	return TRUE;
}

void UmcOnStartComplete(apt_task_t* pTask)
{
	apt_consumer_task_t* pConsumerTask = (apt_consumer_task_t*) apt_task_object_get(pTask);
	UmcFramework* pFramework = (UmcFramework*) apt_consumer_task_object_get(pConsumerTask);
	
	pFramework->LoadScenarios();
	pFramework->CreateMrcpClient();
}

void UmcOnTerminateComplete(apt_task_t* pTask)
{
	apt_consumer_task_t* pConsumerTask = (apt_consumer_task_t*) apt_task_object_get(pTask);
	UmcFramework* pFramework = (UmcFramework*) apt_consumer_task_object_get(pConsumerTask);

	pFramework->DestroyMrcpClient();
	pFramework->DestroyScenarios();
}

apt_bool_t UmcProcessMsg(apt_task_t *pTask, apt_task_msg_t *pMsg)
{
	if(pMsg->type != TASK_MSG_USER)
		return FALSE;

	apt_consumer_task_t* pConsumerTask = (apt_consumer_task_t*) apt_task_object_get(pTask);
	UmcFramework* pFramework = (UmcFramework*) apt_consumer_task_object_get(pConsumerTask);
	UmcTaskMsg* pUmcMsg = (UmcTaskMsg*) pMsg->data;
	switch(pMsg->sub_type) 
	{
		case UMC_TASK_CLIENT_MSG:
		{
			static const mrcp_app_message_dispatcher_t applicationDispatcher = 
			{
				OnSessionUpdate,
				OnSessionTerminate,
				OnChannelAdd,
				OnChannelRemove,
				OnMessageReceive,
				OnReady,
				OnTerminateEvent,
				OnResourceDiscover
			};

			mrcp_application_message_dispatch(&applicationDispatcher,pUmcMsg->m_pAppMessage);
			break;
		}
		case UMC_TASK_RUN_SESSION_MSG:
		{
			if(pFramework->m_Ready)
				pFramework->ProcessRunRequest(pUmcMsg->m_SessionId,pUmcMsg->m_ScenarioName,pUmcMsg->m_ProfileName);
			break;
		}
		case UMC_TASK_KILL_SESSION_MSG:
		{
			if(pFramework->m_Ready)
				pFramework->ProcessKillRequest(pUmcMsg->m_SessionId);
			break;
		}
	}
	return TRUE;
}
