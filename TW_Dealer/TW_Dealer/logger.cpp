//+------------------------------------------------------------------+
//|                                            Logger for Data Feeds |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
//#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#include "sync.h"
#include "logger.h"
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <windows.h>
//#include "Source.h"
//----
#define PREBUF_SIZE    16384
#define COLBUF_SIZE    65536

CLogger ExtLogger;
char    ExtProgramPath[200]="";
//+------------------------------------------------------------------+
//| Constructor                                                      |
//+------------------------------------------------------------------+
CLogger::CLogger(void):m_file(NULL),m_collen(0)
  {
//---- buffers
   m_prebuf=(char*)malloc(PREBUF_SIZE);
   m_colbuf=(char*)malloc(COLBUF_SIZE);
//----
  }
//+------------------------------------------------------------------+
//| Destructor                                                       |
//+------------------------------------------------------------------+
CLogger::~CLogger(void) { Shutdown(); }
//+------------------------------------------------------------------+
//| 青牮噱?腩沣屦 镱腠铖螯?                                      |
//+------------------------------------------------------------------+
void CLogger::Shutdown(void)
  {
   m_sync.Lock();
//----
   FinalizeDay();
//----
   if(m_prebuf!=NULL)  { free(m_prebuf); m_prebuf=NULL;  }
   if(m_colbuf!=NULL)  { free(m_colbuf); m_colbuf=NULL;  }
//----
   m_sync.Unlock();
  }
//+------------------------------------------------------------------+
//| Output logs into the file                                        |
//+------------------------------------------------------------------+
void CLogger::Out(LPCSTR msg,...)
  {
   SYSTEMTIME st;
   va_list    arg_ptr;
   char       tmp[256];
//---- checks and time parsing
   if(msg==NULL || m_prebuf==NULL || m_colbuf==NULL) return;
//---- lets add time
   //begin-------modify----by wzp 2018-05-07
// GetSystemTime(&st);
   GetLocalTime(&st);
   //end---------modify----by wzp 2018-05-07
   va_start(arg_ptr, msg);
   _snprintf_s(tmp, sizeof(tmp)-1, "%04d/%02d/%02d %02d:%02d:%02d ", st.wYear, st.wMonth,st.wDay, st.wHour, st.wMinute, st.wSecond);
//----
   m_sync.Lock();
   Add(tmp);
//---- parse message
   vsprintf_s(m_prebuf, PREBUF_SIZE,msg, arg_ptr);
   va_end(arg_ptr);
   strcat_s(m_prebuf, PREBUF_SIZE,"\r\n");
   Add(m_prebuf);
//----
   m_sync.Unlock();
  }
//+------------------------------------------------------------------+
//| 念徉怆屐 耱痤牦 ?狍翦?                                        |
//+------------------------------------------------------------------+
void CLogger::Add(LPCSTR msg)
  {
   char tmp[256];
   int  len;
//---- 镳钼屦觇 磬 疣珈屦 耦钺龛?
   if(m_colbuf==NULL || msg==NULL) return;
   len=strlen(msg);
   if(len<1 || len>COLBUF_SIZE)    return;
//---- 戾耱?礤 踱囹噱?
  // printf("CLogger::m_collen %s\n", m_collen);
   if((len+m_collen)>=COLBUF_SIZE)
     {
      if(m_collen>0)  // 羼螯 黩?玎镨覃忄螯?
        {
         //---- 铗牮铄?羿殡 羼腓 磬漕
         if(m_file==NULL)
		 {
			 CreateFileName();// add by wzp 2018-06-28
			 _snprintf_s(tmp, sizeof(tmp)-1, "%s.log", m_fileName);
			fopen_s(&m_file, tmp, "ab");
			if (m_file == NULL) return;
           }
         //---- 镳钺箦?玎镨襦螯?
         fwrite(m_colbuf,m_collen,1,m_file);
         m_collen=0;
        }
     }
//---- 眢 ?蝈镥瘘 镳钺箦?漕徉忤螯?
   memcpy(m_colbuf+m_collen,msg,len);
   m_collen+=len;
//----
  }
//+------------------------------------------------------------------+
//| 厌痤??玎牮桢 腩泐?玎 镳邃簌栝 溴睃                        |
//+------------------------------------------------------------------+
void CLogger::FinalizeDay(void)
  {
   char tmp[256];
//---- 眢骓?腓 忸钺 黩?蝾 襻疣覃忄螯?
   if(m_collen<1)
     {
      if(m_file!=NULL) { fclose(m_file); m_file=NULL; }
      return;
     }
//---- ?羿殡 玎 镳邃簌栝 溴睃 铗牮?
   if(m_file==NULL)
     {
	   CreateFileName();// add by wzp 2018-06-28
	   _snprintf_s(tmp, sizeof(tmp)-1, "%s.log", m_fileName);
	  fopen_s(&m_file,tmp, "ab");
     }
//---- 镱镳钺箦?襻痤耔螯 狍翦?磬 滂耜
   if(m_file!=NULL)
     {
      //---- 襻痤耔?镳邃簌栝 狍翦?
      if(m_colbuf!=NULL && m_collen>0) fwrite(m_colbuf,m_collen,1,m_file);
      //---- 玎牮铄?羿殡
      fclose(m_file);
      m_file=NULL;
     }
//---- ?膻犷?耠篦噱 襻痤耔?聍弪麒?溧眄 ?狍翦疱
   m_collen=0;
  }
//+------------------------------------------------------------------+
//| Cut the file                                                     |
//+------------------------------------------------------------------+
void CLogger::Cut(void)
  {
   FILE *in,*out;
   char *buf,*cp,tmp[256];
   int   len;
//---- 玎牮铄?羿殡 磬 怦觇?耠篦嚅
   m_sync.Lock();
   FinalizeDay();
//---- 铗牮铄?羿殡 玎眍忸, 镳钼屦桁 疣珈屦
   CreateFileName();// add by wzp 2018-06-28
   _snprintf_s(tmp, sizeof(tmp)-1, "%s.log", m_fileName);
   fopen_s(&in, tmp, "rb");
   if (in == NULL)        { m_sync.Unlock(); return; }
   len=_filelength(_fileno(in));
   if(len<300000)                        { fclose(in); m_sync.Unlock(); return; }
//---- allocate 100 Kb for last messages and read them
   if((buf=(char*)malloc(102401))==NULL) { fclose(in); m_sync.Unlock(); return; }
   fseek(in,len-102400,SEEK_SET);
   fread(buf,102400,1,in); buf[102400]=0;
   fclose(in);
//---- reopen log-file and write last messages
   fopen_s(&out, tmp, "wb");

   if(out!=NULL)
     {
      if((cp=strstr(buf,"\n"))!=NULL) fwrite(cp+1,strlen(cp+1),1,out);
      fclose(out);
     }
   free(buf);
//----
   m_sync.Unlock();
  }
//+------------------------------------------------------------------+
//| Flushing file & get last 8 Kb                                    |
//+------------------------------------------------------------------+
int CLogger::Journal(char *buffer)
  {
   char *cp,tmp[256];
   int   len,clen;
//---- check
   if(buffer==NULL) return(0);
   m_sync.Lock();
//---- flush file
   FinalizeDay();
//---- reopen file
   CreateFileName();// add by wzp 2018-06-28
   _snprintf_s(tmp, sizeof(tmp)-1, "%s.log", m_fileName);
   fopen_s(&m_file, tmp, "rb");

   if(m_file==NULL)       { m_sync.Unlock(); return(0); }
//---- check the size of file
   if((len=_filelength(_fileno(m_file)))<1) { fclose(m_file); m_file=NULL; m_sync.Unlock(); return(0); }
//---- read last 8 Kb messages
   fseek(m_file,len-1024*8,SEEK_SET);
   len=(len<1024*8)?len:1024*8; // correct length
   if(fread(buffer,len,1,m_file)!=1)        { fclose(m_file); m_file=NULL; m_sync.Unlock(); return(0); }
   fclose(m_file);
   m_file=NULL;
   buffer[len]=0; // null-terminator
//---- find \n from start of buffer
   if((cp=strstr(buffer,"\n"))!=NULL){
      clen=strlen(cp+1);
      memmove(buffer,cp+1,clen); len=clen;
      *(buffer+len)=0;
   }
//---- find \n from end of buffer
   cp=buffer+len-1;
   while(cp>buffer && *cp!=0x0D) { cp--; len--; }
   *cp=0; len--;// terminate last string
//---- return buffer length
   m_sync.Unlock();
   return(len);
  }
//+------------------------------------------------------------------+
// add by wzp 2018-06-28
void CLogger::CreateFileName(){
	SYSTEMTIME st;
	va_list    arg_ptr;
	char       tmp[256];

	GetLocalTime(&st);
	_snprintf_s(tmp, sizeof(tmp)-1, "%s_%04d-%02d-%02d", ExtProgramPath, st.wYear, st.wMonth, st.wDay);
	
	if (strcmp(tmp, m_fileName)){
		memset(m_fileName, 0, sizeof(m_fileName));
		strcpy(m_fileName, tmp);
	}
}