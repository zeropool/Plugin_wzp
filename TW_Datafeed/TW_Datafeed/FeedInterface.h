//+------------------------------------------------------------------+
//|                                                        UniFeeder |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
//|                                                                  |
//|                    DO NOT EDIT THIS FILE!                        |
//|            IT MAY CAUSE ERRORS IN WORK OF A SERVER               |
//+------------------------------------------------------------------+
#pragma once
//+------------------------------------------------------------------+
//| Feeder work mode                                                 |
//+------------------------------------------------------------------+
enum FeederModes
  {
   modeOnlyQuotes    =0,                         // feeder can take quotes only
   modeOnlyNews      =1,                         // feeder can take news only
   modeQuotesAndNews =2,                         // feeder can take quotes and news as well
   modeQuotesOrNews  =3                          // feeder can take quotes or news only
  };
//+------------------------------------------------------------------+
//| Common configuration                                             |
//+------------------------------------------------------------------+
struct ConCommon
  {
   char              owner[128];            // servers owner (include version & build)
   char              name[32];              // server name
   ULONG             address;               // IP address assigned to the server
   int               port;                  // port
   DWORD             timeout;               // sockets timeout
   int               enable_demo;           // enable demo DEMO_DISABLED, DEMO_PROLONG, DEMO_FIXED
   int               timeofdemo;            // demo-account living time (days since last connect)
   int               daylightcorrection;    // allow daylight correction
   char              internal[64];          // reserved
   int               timezone;              // time zone 0-GMT;-1=GMT-1;1=GMT+1;
   char              timesync[64];          // time synchronization server address
   //---
   int               minclient;             // minimal authorized client version
   int               minapi;                // minimal authorized client version
   DWORD             feeder_timeout;        // data feed switch timeout
   int               keepemails;            // internal mail keep period
   int               endhour,endminute;     // end of day time-hour & minute
   //---
   int               optimization_time;     // optimization start time (minutes)
   int               optimization_lasttime; // optimization last time
   int               optimization_counter;  // internal variable
   int               optimization_unused[8];// reserved for future use
   //---
   int               antiflood;             // enable antiflood control
   int               floodcontrol;          // max. antiflood connections
   //---
   int               liveupdate_mode;       // LiveUpdate mode LIVE_UPDATE_NO,LIVE_UPDATE_ALL,LIVE_UPDATE_NO_SERVER
   //---
   int               lastorder;             // last order's ticket     (read only)
   int               lastlogin;             // last account's number   (read only)
   int               lostlogin;             // lost commission's login (read only)
   //---
   int               rollovers_reopen;      // reopen orders at time of rollovers ROLLOVER_NORMAL,ROLLOVER_REOPEN_BY_CLOSE_PRICE,ROLLOVER_REOPEN_BY_BID
   //---
   char              path_database[256];    // path to databases
   char              path_history[256];     // path to history bases
   char              path_log[256];         // path to log
   //--- overnigths
   time_t            overnight_last_day;    // day of last overnight
   time_t            overnight_last_time;   // time of last overnight
   time_t            overnight_prev_time;   // time of previous overnight
   //--- month reports
   time_t            overmonth_last_month;  // time of last report
   //--- performance base
   char              adapters[256];         // network adapters list (read-only)
   ULONG             bind_adresses[8];      // array of available IP addresses
   short             server_version;        // server version (filled by server)
   short             server_build;          // server build (filled by server)
   ULONG             web_adresses[8];       // array of IP addresses available for Web services
   int               statement_mode;        // statements generation mode STATEMENT_END_DAY, STATEMENT_START_DAY
   int               monthly_state_mode;    // monthly generation flag
   int               keepticks;             // ticks keep period
   int               statement_weekend;     // generate statements at weekends
   time_t            last_activate;         // last activation datetime
   time_t            stop_last;             // last stop datetime
   int               stop_delay;            // last stop delay
   int               stop_reason;           // last stop reason
   char              account_url[128];      // account allocation URL
   int               reserved[16];
  };
//+------------------------------------------------------------------+
//| Datafeed configuration                                           |
//+------------------------------------------------------------------+
struct ConFeeder
  {
   char              name[64];              // name
   char              file[256];             // datafeed filename
   char              server[64];            // server address
   char              login[32];             // datafeed login
   char              pass[32];              // datafeed password
   char              keywords[256];         // keywords (news filtration)
   int               enable;                // enable feeder
   int               mode;                  // datafeed mode-enumeration FEED_QUOTES, FEED_NEWS, FEED_QUOTESNEWS
   int               timeout;               // max. freeze time (default ~120 sec.)
   int               timeout_reconnect;     // reconnect timeout before attemps_sleep connect attempts (default ~ 5  sec)
   int               timeout_sleep;         // reconnect timeout after attemps_sleep connect attempts  (default ~ 60 sec)
   int               attemps_sleep;         // reconnect count (see timeout_reconnect & timeout_sleep)
   int               news_langid;           // news language id
   int               unused[33];            // reserved
  };
//--- datafeed modes-receive quotes, receive news, receive quotes and news
enum { FEED_QUOTES=0, FEED_NEWS=1, FEED_QUOTESNEWS=2 };
//--- datafeed flags
enum EnFeederFlags
  {
   FEEDER_FLAGS_NONE       =0,                   // no flags
   FEEDER_FLAGS_NEWS_EXTEND=1,                   // extended news format
  };
//+------------------------------------------------------------------+
//| Feeder description                                               |
//+------------------------------------------------------------------+
struct FeedDescription
  {
   int               version;                    // feeder version
   char              name[128];                  // feeder name
   char              copyright[128];             // copyright string
   char              web[128];                   // web information
   char              email[128];                 // e-mail
   char              server[128];                // communicating server
   char              username[32];               // default login
   char              userpass[32];               // default password
   int               modes;                      // mode (see FeederModes enum)
   char              description[512];           // feeder short description
   char              module[32];                 // datafeed name in license
   int               flags;                      // flags
   int               reserved[52];               // reserved
   int               zero_reserved;              // reserved zero data
  };
//+------------------------------------------------------------------+
//| The data for transfer to a server (quotes and news)              |
//+------------------------------------------------------------------+
#pragma pack(push,1)
struct FeedTick
  {
   char              symbol[16];                 // financial instrument
   time_t            ctm;                        // time
   char              bank[32];                   // bank (source)
   double            bid,ask;                    // quotes
   char              reserved[12];               // reserved
  };
struct FeedData
  {
   //---- ������ ��������?
   FeedTick          ticks[32];
   int               ticks_count;                // incoming quotes amount
   //---- ������ ��������
   char              news_time[32];              // time of news arrival
   char              subject[256];               // news subject (Title)
   char              category[64];               // news category
   char              keywords[256];              // news keywords
   char             *body;                       // news body
   int               body_len;                   // length of news body
   int               body_maxlen;                // maximum length of news body
   //---- ���������� ��������
   int               result;                     // result
   char              result_string[256];         // error description
   int               feeder;                     // feeder index (for internal use)
   int               mode;                       // mode: 0-quotes, 1 -news, 2-news and quotes
   int               langid;                     // news language identifier
   //---
   char              reserved[64];               // reserved
  };
#pragma pack(pop)
//+------------------------------------------------------------------+
//| News structure                                                   |
//+------------------------------------------------------------------+
#pragma pack(push,1)
struct FeedNews
  {
   //--- constants
   enum constants
     {
      MAX_NEWS_BODY_LEN=15*1024*1024                        // max. body len
     };
   //--- news topic flags
   enum EnNewsFlags
     {
      FLAG_PRIORITY    =1,                                  // priority flag
      FLAG_CALENDAR    =2,                                  // calendar item flag
      FLAG_MIME        =4
     };
   UINT              language;                              // news language (WinAPI LANGID)
   wchar_t           subject[256];                          // news subject
   wchar_t           category[256];                         // news category
   UINT              flags;                                 // EnNewsFlags
   wchar_t          *body;                                  // body
   UINT              body_len;                              // body length
   UINT              languages_list[32];                    // list of languages available for news
   INT64             datetime;                              // news time
   UINT              reserved[30];                          // reserved
  };
#pragma pack(pop)
//+------------------------------------------------------------------+
//| Virtual interface                                                |
//+------------------------------------------------------------------+
class CFeedInterface
  {
public:
   //---- virtual methods
   virtual int       Connect(LPCSTR server,LPCSTR login,LPCSTR password)=0;
   virtual void      Close(void)                    =0;
   virtual void      SetSymbols(LPCSTR symbols)     =0;
   virtual int       Read(FeedData *data)           =0;
   virtual int       Journal(char *buffer)          =0;
   virtual int       NewsRead(FeedNews *news)       =0;
   virtual void      NewsFree(FeedNews *news)       =0;
   //----
  };
//+------------------------------------------------------------------+
