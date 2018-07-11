//+------------------------------------------------------------------+
//|                                                        TW_Datafeed |
//|                   Copyright 2014-2018,  TigerWit Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#include "stdafx.h"
#include "Source.h"
//+------------------------------------------------------------------+
//| Description-you can edit it                                      |
//+------------------------------------------------------------------+
#define ProgramVersion 431

static FeedDescription ExtDescription=
  {
   ProgramVersion,                                 // feeder version
   "TW_Datafeed",                                    // feeder name
   "Copyright 2018-2020, TigerWit Software Corp.",  // copyright string
   "http://www.metaquotes.net",                    // web information
   "info@metaquotes.net",                          // e-mail
   "localhost:2222",                               // communicating server
   "localhost",                                    // default login
   "localhost",                                    // default password
   modeQuotesAndNews,                              // mode (see FeederModes enum)
   //---- feeder short description
   "The feeder connects either to Universal DDE Connector for quotations\n"
   "or to eSignal News Server for news.\n\n"

   "For using the feeder, it is necessary to install and set up the proper software.\n"
   "In case of receiving quotations, the names of delivered instruments depend on the  \nDDE server set up.   \n\n"

   "Requirements:\n"
   "\tServer\t :  yes\n"
   "\tLogin\t :  yes\n"
   "\tPassword\t :  yes\n",
   //---
   "",                                             // datafeed name in license
   FEEDER_FLAGS_NEWS_EXTEND                        // flags
   };
//+------------------------------------------------------------------+
//|      Standard wrapper for reenterable datafeed library           |
//|                                                                  |
//|          DO NOT EDIT next lines until end of file                |
//+------------------------------------------------------------------+
#define DATASOURCE_API __declspec(dllexport)
//+------------------------------------------------------------------+
//| Global variables used for feeders list                           |
//+------------------------------------------------------------------+
CSync                ExtSync;
CSourceInterface    *ExtFeeders=NULL;
int Debug_cnt;
//+------------------------------------------------------------------+
//| Entry point                                                      |
//+------------------------------------------------------------------+
BOOL APIENTRY DllMain(HANDLE hModule,DWORD ul_reason_for_call,LPVOID /*lpReserved*/)
  {
   char *cp;
//----
   switch(ul_reason_for_call)
     {
      case DLL_PROCESS_ATTACH:
         //---- parse current folder
		  Debug_cnt = 0;
         GetModuleFileName((HMODULE)hModule,ExtProgramPath,sizeof(ExtProgramPath)-1);
         if((cp=strrchr(ExtProgramPath,'.'))!=NULL) *cp=0;
         //---- initialization message
         ExtLogger.Cut();
         ExtLogger.Out("");
         ExtLogger.Out("%s %d.%02d initialized",ExtDescription.name,ProgramVersion/100,ProgramVersion%100);
         break;
      case DLL_THREAD_ATTACH:  break;
      case DLL_THREAD_DETACH:  break;
      case DLL_PROCESS_DETACH:
         //---- delete all datafeeds
         ExtSync.Lock();
         while(ExtFeeders!=NULL)
           {
            //---- datafeed not closed properly!
            ExtLogger.Out("Unload: datafeed %0X not freed",ExtFeeders);
            CSourceInterface *next=ExtFeeders->Next();
            delete ExtFeeders;
            ExtFeeders=next;
           }
         ExtSync.Unlock();
         break;
     }

   //_CrtDumpMemoryLeaks();
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Create a new datafeed                                            |
//+------------------------------------------------------------------+
DATASOURCE_API CFeedInterface* DsCreate()
  {
   CSourceInterface  *feed;
//---- lock access
   ExtSync.Lock();
//---- create an interface
   ExtLogger.Out("wzp DsCreate: the num %d datafeed object", Debug_cnt);
   if((feed=new(std::nothrow) CSourceInterface)!=NULL)
     {
	  Debug_cnt++;
      //---- insert to list (at first position)
      feed->Next(ExtFeeders);
      ExtFeeders=feed;
     }
  
//---- unlock
   ExtSync.Unlock();
   return((CFeedInterface*)feed);  // return virtual interface
  }
//+------------------------------------------------------------------+
//| Delete datafeed                                                  |
//+------------------------------------------------------------------+
DATASOURCE_API void DsDestroy(CFeedInterface *feed)
  {
   ExtSync.Lock();
//---- find the datafeed
   ExtLogger.Out("wzp DsDestroy: begin.......");
   CSourceInterface *next=ExtFeeders,*last=NULL;
   while(next!=NULL)
     {
      if(next==feed)  // found
        {
         if(last==NULL) ExtFeeders=next->Next();
         else           last->Next(next->Next());
         delete next;
         ExtSync.Unlock();
         return;
        }
      last=next; next=next->Next();
     }
//----
   ExtSync.Unlock();
   ExtLogger.Out("Destroy: unknown datafeed %0X",feed);
  }
//+------------------------------------------------------------------+
//| Request description                                              |
//+------------------------------------------------------------------+
DATASOURCE_API FeedDescription *DsVersion()
  {
   ExtLogger.Out("Feeder flags: %d",ExtDescription.flags);
   return(&ExtDescription);
  }
//+------------------------------------------------------------------+
//| Create a new datafeed                                            |
//+------------------------------------------------------------------+
DATASOURCE_API CFeedInterface*  DsCreateExt(const UINT server_build,const ConFeeder* feed_config,const ConCommon* server_config)
  {
   CSourceInterface  *feed;
//---- lock access
   ExtSync.Lock();
   ExtLogger.Out("wzp DsCreateExt: the num %d datafeed object", Debug_cnt);
//---- create an interface
   if((feed=new(std::nothrow) CSourceInterface)!=NULL)
     {
      //--- extended 
      feed->Extended(ExtDescription.flags & FEEDER_FLAGS_NEWS_EXTEND);
      //---- insert to list (at first position)
	  Debug_cnt++;
      feed->Next(ExtFeeders);
      ExtFeeders=feed;
     }
//---- unlock
   ExtSync.Unlock();
   return((CFeedInterface*)feed);  // return virtual interface
  }
//+------------------------------------------------------------------+
