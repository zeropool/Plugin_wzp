//+------------------------------------------------------------------+
//|                                            Logger for Data Feeds |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#pragma once
//+------------------------------------------------------------------+
//| Ћоггер-рекомендуетс¤ использовать дл¤ фидеро?                   |
//|                                                                  |
//|  лас?создан дл¤ эффективно?работы ?частым выводо?лого?      |
//| ѕреимущества:                                                    |
//|-не делаем лишних перевыделени?па?ти                            |
//|-собирает короткие строки ?отдельны?буфе?m_colbuf) ?выводи?  |
//| их блокам? чт?серьезно уменьшае?фрагментацию файл?           |
//|-автоматическ?подрезае?слишко?длинны?файл? сохран¤¤          |
//| последни?100  б                                                 |
//+------------------------------------------------------------------+
class CLogger
  {
private:
   CSync             m_sync;         // synchronization
   FILE             *m_file;         // file
   char             *m_prebuf;       // buffer for parsing
   char             *m_colbuf;       // buffer for collecting
   int               m_collen;       // size of collected data

public:
                     CLogger(void);
                    ~CLogger(void);
   void              Shutdown(void);
   //---- logging helpers
   void              Out(LPCSTR msg,...);
   int               Journal(char *buffer);
   void              Cut(void);

private:
   void              FinalizeDay(void);
   void              Add(LPCSTR msg);
  };
//---- global object
extern CLogger ExtLogger;
extern char    ExtProgramPath[200];
//+------------------------------------------------------------------+
