//+------------------------------------------------------------------+
//|                                            Logger for Data Feeds |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#pragma once
#include <stdio.h>
#include "sync.h"
//+------------------------------------------------------------------+
//| 祟沣屦-疱觐戾礓箦蝰 桉镱朦珙忄螯 潆 翳溴痤?                   |
//|                                                                  |
//| 孰囫?耦玟囗 潆 翦牝桠眍?疣犷螓 ?鬣耱 恹忸漕?腩泐?      |
//| 橡彖祗耱忄:                                                    |
//|-礤 溴豚屐 腓桴 镥疱恹溴脲龛?镟?蜩                            |
//|-耦徼疣弪 觐痤蜿桢 耱痤觇 ?铗溴朦睇?狍翦?m_colbuf) ?恹忸滂?  |
//| 桴 犭铌囔? 黩?皴瘘彗眍 箪屙噱?麴嚆戾眚圉棹 羿殡?           |
//|-噔蝾爨蜩麇耜?镱漯彗噱?耠桫觐?潆桧睇?羿殡? 耦躔囗          |
//| 镱耠邃龛?100 梳                                                 |
//+------------------------------------------------------------------+
class CLogger
  {
private:
	CSync        m_sync;         // synchronization
	FILE        *m_file;         // file
    char        *m_prebuf;       // buffer for parsing
    char        *m_colbuf;       // buffer for collecting
    int         m_collen;        // size of collected data
public:
	char		m_fileName[256];
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
   void				 CreateFileName();//add by wzp 2018-06-28
  };
//---- global object
//extern CLogger ExtLogger;
extern char    ExtProgramPath[200];
//+------------------------------------------------------------------+
