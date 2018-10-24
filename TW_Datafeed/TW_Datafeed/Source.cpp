//+------------------------------------------------------------------+
//|                                                        UniFeeder |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#include "stdafx.h"
#include "Source.h"

#define BUFFER_SIZE 128*1024
//+------------------------------------------------------------------+
//| News header structure                                            |
//+------------------------------------------------------------------+
struct NewsTopic
  {
   char              time[20];        // time in "yyyy/mm/dd hh:mm:ss" format
   char              topic[256];      // subject (topic)
   char              keywords[256];   // keywords
   int               len;             // message length
   char              unused[12];       // unused data (left for compatibility)
  };
//+------------------------------------------------------------------+
//| Constructor                                                      |
//+------------------------------------------------------------------+
CSourceInterface::CSourceInterface(void) : m_socket(INVALID_SOCKET),m_symbols(NULL),
                                           m_loginflag(FALSE),m_lasttime(0),m_extended_mode(false),
                                           m_waiting_news(FALSE),m_next(NULL),
                                           m_data(NULL),m_data_max(0),m_data_readed(0),m_data_total(0),
                                           m_read_news_count(0),m_free_news_count(0)

  {
//----
   m_login[0]=0;
   m_password[0]=0;
   m_server[0]=0;
   m_buffer=(char*)malloc(BUFFER_SIZE);
//----
  }
//+------------------------------------------------------------------+
//| Destructor                                                       |
//+------------------------------------------------------------------+
CSourceInterface::~CSourceInterface(void)
  {
//---- close connection
   Close();
//---- remove the buffer with the list of instruments
   if(m_symbols!=NULL) { free(m_symbols); m_symbols=NULL; }
   if(m_buffer!=NULL)  { free(m_buffer);  m_buffer =NULL; }
   if(m_data!=NULL)    { free(m_data);    m_data   =NULL; }
   m_data_max=m_data_readed=m_data_total=0;
//----
   m_next=NULL;
  }
//+------------------------------------------------------------------+
//| Connecting to server                                             |
//+------------------------------------------------------------------+
int CSourceInterface::Connect(LPCSTR server,LPCSTR login,LPCSTR password)
  {
   unsigned int       addr;
   struct sockaddr_in srv;
   struct hostent    *hp;
   char              *cp,ip[128];
   int                port=2222;
//---- checks
   m_loginflag=FALSE;
   m_waiting_news=FALSE;
   if(server==NULL || login==NULL || password==NULL) return(FALSE);
//---- save data for future use
   COPY_STR(m_server,server); COPY_STR(ip,m_server);
   COPY_STR(m_login,login);
   COPY_STR(m_password,password);
   if((cp=strstr(ip,":"))!=NULL) { port=atoi(cp+1); *cp=0; }
//---- close connection
   Close();
//---- create a socket of a server for work with the dataflow
   if((m_socket=socket(AF_INET, SOCK_STREAM,0))==INVALID_SOCKET)
     {
      ExtLogger.Out("Connect: socket error");
      return(FALSE);
     }
//----
   ZeroMemory(&srv,sizeof(srv));
   if((addr=inet_addr(ip))==INADDR_NONE)
     {
      if((hp=gethostbyname(ip))==NULL)
        {
         Close();
         ExtLogger.Out("Connect: gethostbyname failed %s",ip);
         return(FALSE);
        }
      srv.sin_addr.s_addr=*((unsigned long*)hp->h_addr);
     }
   else srv.sin_addr.s_addr=addr;

   srv.sin_family=AF_INET;
   srv.sin_port=htons(port);
//---- connect to server
   if(connect(m_socket,(struct sockaddr*)&srv,sizeof(srv))!=0)
     {
      Close();
      ExtLogger.Out("Connect: failed %s",m_server);
      return(FALSE);
     }
   ExtLogger.Out("Connect: successful to %s",m_server);
//---- load symbols
 //  m_syntetics.Load(); del by wzp 2018-03-28
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Closing connection                                               |
//+------------------------------------------------------------------+
void CSourceInterface::Close(void)
  {
//----
   m_loginflag=FALSE;
   m_waiting_news=FALSE;
   if(m_socket!=INVALID_SOCKET)
     {
      shutdown(m_socket,2);
      closesocket(m_socket);
      m_socket=INVALID_SOCKET;
     }
//----
  }
//+------------------------------------------------------------------+
//| Set prefered instruments                                         |
//+------------------------------------------------------------------+
void CSourceInterface::SetSymbols(LPCSTR symbols)
  {
//---- remove the buffer with the list of instruments
   if(m_symbols!=NULL)
     {
      free(m_symbols);
      m_symbols=NULL;
     }
//---- set new list
   if(symbols!=NULL)
     {
      m_symbols=(char*)malloc(strlen(symbols)+10);
      if(m_symbols!=NULL) strcpy(m_symbols,symbols);
      ExtLogger.Out("Symbols: %s",symbols);
     }
//----
  }
//+------------------------------------------------------------------+
//| Socket readable checking                                         |
//+------------------------------------------------------------------+
int CSourceInterface::IsReadible(void)
  {
   unsigned long size=0;
//---
   if(m_socket!=INVALID_SOCKET)
      if(ioctlsocket(m_socket,FIONREAD,&size)!=0)
        {
         Close();
         size=0;
        }
//---
   return(size);
  }
//+------------------------------------------------------------------+
//| Send string                                                      |
//+------------------------------------------------------------------+
bool CSourceInterface::SendString(LPCSTR buf)
  {
   int count=0,res;
//---- check
   if(buf==NULL || m_socket==INVALID_SOCKET) return(FALSE);
//---- send data
   int len=strlen(buf);
   while(len>0)
     {
      if((res=send(m_socket,buf,len,0))<1)
        {
         //---- check fatal error
         if(WSAGetLastError()!=WSAEWOULDBLOCK || count>10)
           {
            Close();
            return(FALSE);
           }
         //---- write blocking, wait and read further
         count++;
         Sleep(50);
         continue;
        }
      buf+=res; len-=res;
     }
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read string                                                      |
//+------------------------------------------------------------------+
bool CSourceInterface::ReadString(char *buf,const int maxlen)
  {
   int  count=0,len=0;
//---- checks
   if(buf==NULL || m_socket==INVALID_SOCKET) return(FALSE);
//---- read data
   while(len<maxlen)
     {
      if(recv(m_socket,buf,1,0)!=1)
        {
         //---- check fatal error
         if(WSAGetLastError()!=WSAEWOULDBLOCK || count>10)
           {
            Close();
            return(FALSE);
           }
         //---- read blocking, wait and read further
         count++;
         Sleep(50);
         continue;
        }
      if(*buf==13) continue;
      if(*buf==10) break;
      len++; buf++;
     }
   *buf=0;
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read datablock of the set length                                 |
//+------------------------------------------------------------------+
bool CSourceInterface::ReadData(char *buf,int len)
  {
   int  count=0,ln;
//---- checks
   if(buf==NULL || m_socket==INVALID_SOCKET) return(FALSE);
//---- read data
   while(len>0)
     {
      if((ln=recv(m_socket,buf,len,0))<1)
        {
         //---- check fatal error
         if(WSAGetLastError()!=WSAEWOULDBLOCK || count>10)
           {
            Close();
            return(FALSE);
           }
         //---- read blocking, wait and read further
         count++;
         Sleep(50);
         continue;
        }
      len-=ln;
      buf+=ln;
     }
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read string and find substring                                   |
//+------------------------------------------------------------------+
int CSourceInterface::ReadCheckString(char *buf,const int maxlen,LPCSTR string)
  {
   int   count=0,len=0;
   char *savebuf=buf;
//---- checks
   if(buf==NULL || m_socket==INVALID_SOCKET || string==NULL) return(-1);
//---- read data
   while(len<maxlen)
     {
      if(recv(m_socket,buf,1,0)!=1)
        {
         //---- check fatal error
         if(WSAGetLastError()!=WSAEWOULDBLOCK || count>10)
           {
            Close();
            return(-1);
           }
         //---- read blocking, wait and read further
         count++;
         Sleep(50);
         continue;
        }
      if(*buf==13) continue;
      if(*buf==10) break;
      len++;
      buf++;
      *buf=0;
      //---- compare strings
      if(strstr(savebuf,string)!=NULL) return(1); // found!
     }
   *buf=0;
//---- string readed, but substring not found
   return(0);
  }
//+------------------------------------------------------------------+
//| Authorization                                                    |
//+------------------------------------------------------------------+
bool CSourceInterface::Login(FeedData *inf)
  {
  // char  tmp[260];
   //int   res,count=0;
//---- checks
   if (inf == NULL) {
	   ExtLogger.Out("wzp Login: (inf == NULL)\n");   //add by wzp 2018-03-26
	   return(FALSE);
   }
   m_loginflag=FALSE;
   m_waiting_news=FALSE;
//begin------------del login comunication-------by wzp 20180328
//---- wait for login
   //for(;;)
   //  {
   //   if((res=ReadCheckString(tmp,sizeof(tmp)-1,"Login: "))<0)
   //     {
   //      strcpy(inf->result_string,"login failed [login field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //   if(res==1) // found
   //     {
   //      _snprintf(tmp,sizeof(tmp)-1,"%s\r\n",m_login);
   //      if(SendString(tmp)==FALSE)
   //        {
   //         strcpy(inf->result_string,"connection lost");
   //         ExtLogger.Out("Login: %s",inf->result_string);
   //         return(FALSE);
   //        }
   //      break;
   //     }
   //   count++;
   //   if(count>3)
   //     {
   //      strcpy(inf->result_string,"invalid headers [login field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //  }
//---- wait for password
   //count=0;
   //for(;;)
   //  {
   //   if((res=ReadCheckString(tmp,sizeof(tmp)-1,"Password: "))<0)
   //     {
   //      strcpy(inf->result_string,"login failed [password field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //   if(res==1) // found
   //     {
   //      _snprintf(tmp,sizeof(tmp)-1,"%s\r\n",m_password);
   //      if(SendString(tmp)!=TRUE)
   //        {
   //         strcpy(inf->result_string,"connection lost");
   //         ExtLogger.Out("Login: %s",inf->result_string);
   //         return(FALSE);
   //        }
   //      break;
   //     }
   //   count++;
   //   if(count>3)
   //     {
   //      strcpy(inf->result_string,"invalid headers [password field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //  }
//---- read answer
   //count=0;
   //for(;;)
   //  {
   //   if((res=ReadCheckString(tmp,sizeof(tmp)-1,"Access "))<0)
   //     {
   //      strcpy(inf->result_string,"login failed [access field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //   if(res==1) break;
   //   count++;
   //   if(count>3)
   //     {
   //      strcpy(inf->result_string,"invalid headers [access field]");
   //      ExtLogger.Out("Login: %s",inf->result_string);
   //      return(FALSE);
   //     }
   //  }
//----
   //if(ReadString(tmp,sizeof(tmp)-1)!=TRUE)
   //  {
   //   strcpy(inf->result_string,"connection lost");
   //   ExtLogger.Out("Login: %s",inf->result_string);
   //   return(FALSE);
   //  }
   //if(strstr(tmp,"granted")==NULL)
   //  {
   //   _snprintf(inf->result_string,sizeof(inf->result_string)-1,"access %s",tmp);
   //   ExtLogger.Out("Login: %s",inf->result_string);
   //   return(FALSE);
   //  }
//end------------del login comunication-------by wzp 20180328
//---- login succesful
   m_loginflag=TRUE;
   m_lasttime=GetTickCount();
   ExtLogger.Out("Login: '%s' successful",m_login);
//---- drop incoming buffer
   m_data_total=m_data_readed=0;
   m_data_max = 256; //set max specific value and don't change the value add by wzp  2018-03-28
//---- send instruments list
   if(m_symbols!=NULL)
      if(m_symbols[0]!=0)
        {

         if(SendString("> Symbols:")==TRUE && SendString(m_symbols)==TRUE && SendString("\r\n")==TRUE)
            return(TRUE);
         //---- send of prefered instruments failed
         strcpy(inf->result_string,"symbols initialization failed");
         ExtLogger.Out("Login: %s",inf->result_string);
         return(FALSE);
        }
//----
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Main data reading function                                       |
//+------------------------------------------------------------------+
int CSourceInterface::Read(FeedData *inf)
  {
   DWORD   ctm;
//---- check
   if(inf==NULL || m_buffer==NULL)      return(FALSE);
   inf->ticks_count=0;
//---- есть синтетически?инструмент?
//   if(m_syntetics.GetTicks(inf)!=FALSE) return(TRUE); del by wzp 2018-03-28
//---- is connection opened?
   if(m_socket==INVALID_SOCKET)
     {
      //---- try connect
      if(Connect(m_server,m_login,m_password)==FALSE)
        {
         _snprintf(inf->result_string,sizeof(inf->result_string)-1,"no connection %s",m_server);
         inf->result=FALSE;
         return(FALSE);
        }
      if(Login(inf)==FALSE) return(FALSE);
     }
//---- is login succesful?
   if(m_loginflag==FALSE)
      if(Login(inf)==FALSE) return(FALSE);
//--- if waiting for news, skip this turn
   if(m_waiting_news)       return(TRUE);
//---- read data
   while(m_socket!=INVALID_SOCKET)
     {
      //---- send ping every minute
      ctm=GetTickCount();
      if(ctm<m_lasttime) m_lasttime=ctm-1000;
      if((ctm-m_lasttime)>60000)
        {
         //---- load syntetic base
  //       m_syntetics.Load(); del by wzp 2018-03-28
         //----
         m_lasttime=ctm;
         if(SendString("> Ping\r\n")==FALSE)
           {
            strcpy(inf->result_string,"ping failed");
            ExtLogger.Out("Read: %s",inf->result_string);
            return(FALSE);
           }
        }
      //---- check data
      if(DataRead()==FALSE)
        {
         COPY_STR(inf->result_string,"connection lost");
         ExtLogger.Out("Read: %s",inf->result_string);
         return(FALSE);
        }
      //---- parse while we can read strings
      while(DataParseLine(m_buffer,BUFFER_SIZE-1))
        {
         //---- it is news?
         if(memcmp(m_buffer,"< News",6)==0)
           {
            //--- if not extended mode - process news itself
            if(!m_extended_mode)
               return ReadNews(inf);
            else
              {
               //--- if extended mode - wait until NewsRead process news
               m_waiting_news=true;
               return(TRUE);
              }
           }
         //---- skip service information
         if(m_buffer[0]=='<' || m_buffer[0]=='>') continue;
         //---- parse ticks
         if(!ReadTicks(m_buffer,inf))
           {
            ExtLogger.Out("Read: ReadTicks failed");
            return(FALSE);
           }
         //---- check free space for new account
         if(inf->ticks_count<COUNTOF(inf->ticks)) continue;
         //---- break parsing
         break;
        }
      //---- check ticks
      if(inf->ticks_count>0)
        {
         //---- update syntetics
//         m_syntetics.AddQuotes(inf);del by wzp 2018-03-28
         //---- return data to server
         return(TRUE);
        }
      //----
      Sleep(10);
     }
//---- some error
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Main news reading function                                       |
//+------------------------------------------------------------------+
int CSourceInterface::NewsRead(FeedNews *news)
  {
//--- check
   if(news==NULL || m_buffer==NULL)      return(FALSE);
//--- clear result
   ZeroMemory(news,sizeof(*news));
//--- no news
   if(!m_waiting_news)
     {
      return(TRUE);
     }
//--- process news
   m_waiting_news=false;
   return(ReadNews(news));
  }
//+------------------------------------------------------------------+
//| Free news body                                                   |
//+------------------------------------------------------------------+
void CSourceInterface::NewsFree(FeedNews *news)
  {
//--- check
   if(news==NULL || news->body==NULL)
      return;
//--- delete body
   delete[] news->body;
   news->body=NULL;
  }
//+------------------------------------------------------------------+
//| Read ticks data                                                  |
//+------------------------------------------------------------------+
bool CSourceInterface::ReadTicks(LPCSTR ticks,FeedData *inf)
  {
   char   *cp;
//---- checks
   if(ticks==NULL || inf==NULL || inf->ticks_count>=COUNTOF(inf->ticks)) return(FALSE);
   if(m_buffer==NULL)                                                    return(FALSE);
//---- read prices, string like: USDJPY 0 1.0106 1.0099
//---- define symbol
   if((cp=strstr((char*)ticks," "))==NULL)     return(TRUE);
   *cp=0;
   COPY_STR(inf->ticks[inf->ticks_count].symbol,m_buffer);
//---- check for time
   char *np=cp+1;
   int   params=1;
   while(*np!=0)
     {
      if(*np==' ') params++;
      np++;
     }
   if(params>2) cp=strstr(cp+1," ");  // skip [time] parameter
//---- define prices
   inf->ticks[inf->ticks_count].bid=atof(cp+1);
   if((cp=strstr(cp+1," "))==NULL)      return(TRUE);
   inf->ticks[inf->ticks_count].ask=atof(cp+1);
//---- check prices
   if(inf->ticks[inf->ticks_count].bid<=0 || inf->ticks[inf->ticks_count].ask<=0)
   {
      _snprintf(inf->result_string,sizeof(inf->result_string)-1,"Read: invalid bid/ask '%s'",inf->ticks[inf->ticks_count].symbol);
      ExtLogger.Out("%s",inf->result_string);
      return(FALSE);
    }

   //add by wzp if bid>ask don't reconnect the link we don't send the data to mt4
   if (inf->ticks[inf->ticks_count].bid > inf->ticks[inf->ticks_count].ask){
	   _snprintf(inf->result_string, sizeof(inf->result_string) - 1, "Read: invalid bid/ask '%s'", inf->ticks[inf->ticks_count].symbol);
	   ExtLogger.Out("%s", inf->result_string);
	   inf->ticks[inf->ticks_count].bid = 0.0;
	   inf->ticks[inf->ticks_count].ask = 0.0;
	   memset(inf->ticks[inf->ticks_count].symbol, 0, sizeof(inf->ticks[inf->ticks_count].symbol));
	   inf->ticks_count--;
   }

   inf->ticks_count++;
//---- return succesful result
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read news data                                                   |
//+------------------------------------------------------------------+
bool CSourceInterface::ReadNews(FeedData *inf)
  {
   NewsTopic  news;
//---- checks
   if(inf==NULL)       return(FALSE);
   if(inf->body==NULL) return(FALSE);
//---- read
   if(DataParseData((char*)&news,sizeof(NewsTopic))!=TRUE)
     {
      COPY_STR(inf->result_string,"connection lost");
      ExtLogger.Out("ReadNews: %s",inf->result_string);
      return(FALSE);
     }
//---- put restrictions
   news.topic[sizeof(news.topic)-1]=0;
   news.keywords[sizeof(news.keywords)-1]=0;
   news.time[sizeof(news.time)-1]=0;
//---- copy news to the buffer
   COPY_STR(inf->subject  ,news.topic);
   COPY_STR(inf->category ,news.keywords);
   COPY_STR(inf->news_time,news.time);
//---- news have not body?
   if(news.len==0)
     {
      inf->body[0]=0;
      inf->body_len=0;
      return(TRUE);
     }
//---- check for maximum body length
   if(news.len<0 || news.len>inf->body_maxlen)
     {
      _snprintf(inf->result_string,sizeof(inf->result_string)-1,"too long news body [%d / %d]",news.len,inf->body_maxlen);
      ExtLogger.Out("ReadNews: %s",inf->result_string);
      return(FALSE);
     }
//---- read news body
   if(DataParseData(inf->body,news.len)!=TRUE)
     {
		  COPY_STR(inf->result_string,"connection lost");
		  ExtLogger.Out("ReadNews: %s",inf->result_string);
		  return(FALSE);
     }

   inf->body_len=news.len;
   inf->body[news.len]=0;
//---- succesful
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read news data                                                   |
//+------------------------------------------------------------------+
bool CSourceInterface::ReadNews(FeedNews *inf)
  {
   NewsTopic  news;
//---- checks
   if(inf==NULL)       return(FALSE);
   inf->body=NULL;
//---- read
   if(DataParseData((char*)&news,sizeof(NewsTopic))!=TRUE)
     {
      ExtLogger.Out("ReadNews: connection lost");
      return(FALSE);
     }
//---- put restrictions
   news.topic[sizeof(news.topic)-1]=0;
   news.keywords[sizeof(news.keywords)-1]=0;
   news.time[sizeof(news.time)-1]=0;
//---- copy news to the buffer
   ::MultiByteToWideChar(CP_UTF8,0,news.topic,-1,inf->subject,_countof(inf->subject));
   ::MultiByteToWideChar(CP_UTF8,0,news.keywords,-1,inf->category,_countof(inf->category));
//---- news have not body?
   if(news.len==0)
     {
      inf->body=NULL;
      inf->body_len=0;
      return(TRUE);
     }
//---- check for maximum body length
   if(news.len<0 || news.len>=inf->MAX_NEWS_BODY_LEN)
     {
      ExtLogger.Out("ReadNews: too long news body [%d / %d]",news.len,inf->MAX_NEWS_BODY_LEN);
      return(FALSE);
     }
//--- allocating temporary buffer
   char* tmp=new char[news.len+1];
//---- read news body
   if(DataParseData(tmp,news.len)!=TRUE)
     {
      ExtLogger.Out("ReadNews: connection lost");
      delete[]tmp;
      return(FALSE);
     }
//--- terminating buffer
   tmp[news.len]='\0';
//---- calculate wide char body length
   inf->body_len=::MultiByteToWideChar(CP_UTF8,0,tmp,-1,NULL,0);
   if(inf->body_len==0)
     {
      ExtLogger.Out("ReadNews: failed to convert text format");
      delete[]tmp;
      return(FALSE);
     }
//--- copy news body
   inf->body=new wchar_t[inf->body_len+1];
   if(::MultiByteToWideChar(CP_UTF8,0,tmp,-1,inf->body,(inf->body_len+1))==0)
     {
      ExtLogger.Out("ReadNews: failed to convert text format");
      delete[]tmp;
      delete[]inf->body;
      inf->body=NULL;
      return(FALSE);
     }
   inf->body[inf->body_len]=L'\0';
   delete[]tmp;
//---- succesful
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Read all available data from socket                              |
//+------------------------------------------------------------------+
int CSourceInterface::DataRead(void)
  {
   int   len,res;
  // char *cp;
//--- check data in socket
   if((len=IsReadible())<1) return(TRUE);
//--- move unparsed data
   if(m_data!=NULL && m_data_readed>0)
   {
      if((m_data_total-m_data_readed)>0) memmove(m_data,m_data+m_data_readed,m_data_total-m_data_readed);
      m_data_total-=m_data_readed;
      m_data[m_data_total]=0;
      m_data_readed=0;
   }
//--- check free space
   //begin-------del: can not enlarge the m_data cap len------del by wzp 2018-03-28-----
   //if((m_data_total+len)>m_data_max || m_data==NULL)
   //  {
   //   //--- check buffer size
   //   if((m_data_total+len)>READ_BUFFER_MAX)
   //     {
   //      ExtLogger.Out("Read: incoming buffer too large %d bytes",m_data_total+len);
   //      return(FALSE);
   //     }
   //   //--- allocate memory
   //   if((cp=(char*)malloc(m_data_total+len+READ_BUFFER_STEP+16))==NULL)
   //     {
   //      ExtLogger.Out("Read: not enough memory %d bytes",m_data_total+len+1024);
   //      return(FALSE);
   //     }
   //   //--- copy old buffer
   //   if(m_data!=NULL)
   //     {
   //      if(m_data_total>0) memcpy(cp,m_data,m_data_total);
   //      free(m_data);
   //     }
   //   m_data=cp;
   //   m_data_max=m_data_total+len+READ_BUFFER_STEP;
   //  }
   //end-------del: can not enlarge the m_data cap len------del by wzp 2018-03-28-----
   //Begin---add  m_data  allocation--add by wzp 2018-03-28-----
   if (NULL == m_data){
	   m_data = (char*)malloc(m_data_max);
   }
   //End---add  m_data  allocation --add by wzp 2018-03-28-----
//--- read data to buffer
//begin----modify by wzp :print  messy code . 2018-04-25
   if ((res = recv(m_socket, m_data + m_data_total, m_data_max - m_data_total -1, 0))<1) return(FALSE); //modify by wzp 2018-03-29 the buffer read len set
   m_data_total+=res;
//   ExtLogger.Out("wzp Read: real len %d", res);
   m_data[m_data_total]=0;
   ExtLogger.Out("wzp m_data:%s", m_data);
//end------modify by wzp :print  messy code . 2018-04-25
//--- return
   return(TRUE);
  }
//+------------------------------------------------------------------+
//| Parse string from buffer                                         |
//+------------------------------------------------------------------+
int CSourceInterface::DataParseLine(char *buf,const int maxlen)
  {
//--- check params
   if(buf==NULL || maxlen<1 || m_data==NULL || m_data_total<1) return(FALSE);
//--- prepare data
   int   len=0;
   char *start=m_data+m_data_readed;
   char *end=m_data+m_data_total;
//--- parse string from buffer
   while(start<end && len<maxlen)
     {
      //--- check string end
      if(*start=='\n')
        {
         //--- set string end
         *buf=0;
         m_data_readed+=len+1;
         //--- ok
         return(TRUE);
        }
      //--- copy data skipping '\r'
      if(*start!='\r') *buf++=*start;
      start++; len++;
     }
//--- can't read string
   return(FALSE);
  }
//+------------------------------------------------------------------+
//|  Data parse                                                      |
//+------------------------------------------------------------------+
int CSourceInterface::DataParseData(char *data,const int len)
  {
//--- check params
   if(data==NULL || len<1 || m_data==NULL) return(FALSE);
//---- 
   if((m_data_total-m_data_readed)>=(UINT)len)
     {
      memcpy(data,m_data+m_data_readed,len);
      m_data_readed+=len;
     }
   else
     {
      //--- need to read some data from socket
      int buf_data_size=m_data_total-m_data_readed;
      if(buf_data_size>0)
        {
         memcpy(data,m_data+m_data_readed,buf_data_size);
         m_data_readed+=buf_data_size;
        }
      //--- read remainder
      if(ReadData(data+buf_data_size,len-buf_data_size)==FALSE) return(FALSE);
     }
//---
   return(TRUE);
  }
//+------------------------------------------------------------------+
