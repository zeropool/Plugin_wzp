//+------------------------------------------------------------------+
//|                                                        UniFeeder |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#pragma once

//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
#define READ_BUFFER_MAX  (1024*1024)
#define READ_BUFFER_STEP (16*1024)
//+------------------------------------------------------------------+
//| Datafeed interface implementation                                |
//+------------------------------------------------------------------+
class CSourceInterface : public CFeedInterface
  {
protected:
  // CSynteticBase     m_syntetics;      // ���� ������������?������������ del by wzp 2018-03-28 delete base file relation content
   SOCKET            m_socket;         // communicating socket
   char              m_server[128];    // communicating server
   char              m_login[32];      // authorization login
   char              m_password[32];   // authorization password
   char             *m_symbols;        // instruments list
   bool              m_loginflag;      // login flag
   DWORD             m_lasttime;       // ping last time
   char             *m_buffer;         // read/write buffer
   bool              m_extended_mode;  // running with extended mode
   bool              m_waiting_news;   // waiting for process news
   //--- readed data
   char             *m_data;           // data buffer
   UINT              m_data_max;       // max buffer length
   UINT              m_data_readed;    // readed data size
   UINT              m_data_total;     // data to read
   //----
   CSourceInterface *m_next;           // next interface pointer (do not delete!)
   //--- counters
   UINT              m_read_news_count;// counter for NewsRead function
   UINT              m_free_news_count;// counter for NewsFree function

public:
                     CSourceInterface(void);
   virtual          ~CSourceInterface(void);
   //---- search helpers (do not delete)
   inline CSourceInterface* Next(void) const      { return(m_next); }
   inline void       Next(CSourceInterface *next) { m_next=next;    }
   inline void       Extended(bool extended)      { m_extended_mode=extended; }
   //---- implementations (do not delete)
   virtual int       Connect(LPCSTR server,LPCSTR login,LPCSTR password);
   virtual void      Close(void);
   virtual void      SetSymbols(LPCSTR symbols);
   virtual int       Read(FeedData *data);
   virtual int       Journal(char *buffer) { return ExtLogger.Journal(buffer); }
   virtual int       NewsRead(FeedNews *news);
   virtual void      NewsFree(FeedNews *news);

protected:
   //---- internals
   bool              Login(FeedData *inf);
   int               IsReadible(void);
   bool              ReadData(char *buf,int len);
   bool              SendString(LPCSTR buf);
   bool              ReadString(char *buf,const int maxlen);
   int               ReadCheckString(char *buf,const int maxlen,LPCSTR string);
   bool              ReadNews(FeedData *inf);
   bool              ReadNews(FeedNews *inf);
   bool              ReadTicks(LPCSTR ticks,FeedData *inf);
   //----
   int               DataRead(void);
   int               DataParseLine(char *buf,const int maxlen);
   int               DataParseData(char *data,const int len);
  };
//+------------------------------------------------------------------+
