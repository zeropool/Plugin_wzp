#include "DealerService.h"
#include "get_config.h"

using namespace std;

#define SLEEP_TIME 100

SERVICE_STATUS servicestatus;

SERVICE_STATUS_HANDLE hstatus;

void WINAPI ServiceMain(int argc, char** argv);

void WINAPI CtrlHandler(DWORD request);

int start_service();

int InitService();

bool brun = false;

void WINAPI ServiceMain(int argc, char** argv)
{
	servicestatus.dwServiceType = SERVICE_WIN32;

	servicestatus.dwCurrentState = SERVICE_START_PENDING;

	servicestatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;

	servicestatus.dwWin32ExitCode = 0;

	servicestatus.dwServiceSpecificExitCode = 0;

	servicestatus.dwCheckPoint = 0;

	servicestatus.dwWaitHint = 0;

	hstatus = ::RegisterServiceCtrlHandler("DealerService", CtrlHandler);

	if (hstatus == 0)
	{
		return;
	}

	servicestatus.dwCurrentState = SERVICE_RUNNING;

	SetServiceStatus(hstatus, &servicestatus);

	if (0 == start_service()){
		return;
	}

	while (!brun){
		Sleep(SLEEP_TIME);
	}
}

void WINAPI CtrlHandler(DWORD request)
{

	switch (request)
	{
	case SERVICE_CONTROL_STOP:
		servicestatus.dwCurrentState = SERVICE_STOPPED;
		brun = true;
		break;

	case SERVICE_CONTROL_SHUTDOWN:
		servicestatus.dwCurrentState = SERVICE_STOPPED;
		brun = true;
		break;
	default:
		break;
	}

	SetServiceStatus(hstatus, &servicestatus);
}

int start_service(){
	
	//get instance and  some init operate:Note good
	if (NULL == DealerService::GetInstance()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "NULL == DealerService::GetInstance()");
		OutputDebugString("ERR:NULL == DealerService::GetInstance()");
		return 0;
	}
	//create mt4 link:Note good
	if (!DealerService::GetInstance()->CreateMT4Link()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "MT4:GetInstance()->CreateMT4Link failed");
		OutputDebugString("ERR:MT4 :GetInstance()->CreateMT4Link failed");
		return 0;
	}
	//create bridge link:Note good
	if (!DealerService::GetInstance()->CreateBridgeLink()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Bridge:GetInstance()->CreateBridgeLink failed");
		OutputDebugString("ERR:Bridge:GetInstance()->CreateBridgeLink failed");
		return 0;
	}

	//init thread note:read message from bridge then send the message to mt4
	DealerService::GetInstance()->InitThread();
	//switch mode for dealer and pump.
	DealerService::GetInstance()->SwitchMode();

	//system("pause");
	return 1;
}


int main(int argc, char *argv[]){
	start_service();

	/*SERVICE_TABLE_ENTRY entrytable[2];

	entrytable[0].lpServiceName = "DealerService";

	entrytable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

	entrytable[1].lpServiceName = NULL;

	entrytable[1].lpServiceProc = NULL;

	StartServiceCtrlDispatcher(entrytable);*/

	//system("pause");
	return 0;
}