// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved){
	char tmp[256], *cp;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//--- create configuration filename
		GetModuleFileName((HMODULE)hModule, tmp, sizeof(tmp)-5);
		if ((cp = strrchr(tmp, '.')) != NULL) { *cp = 0; strcat(tmp, ".ini"); }
		//--- load configuration
		ExtConfig.Load(tmp);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

