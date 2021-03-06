//+------------------------------------------------------------------+
//|                                            MetaTrader Server API |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#include "stdafx.h"
#include "stringfile.h"
//+------------------------------------------------------------------+
//| 暑眈蝠箨蝾?                                                     |
//+------------------------------------------------------------------+
CStringFile::CStringFile(const int nBufSize) :
m_file(INVALID_HANDLE_VALUE), m_file_size(0),
m_buffer(new BYTE[nBufSize]), m_buffer_size(nBufSize),
m_buffer_index(0), m_buffer_readed(0), m_buffer_line(0)
{
}
//+------------------------------------------------------------------+
//| 腻耱痼牝铕                                                       |
//+------------------------------------------------------------------+
CStringFile::~CStringFile()
{
	//--- 玎牮铄?耦邃桧屙桢
	Close();
	//--- 铖忸犷滂?狍翦?
	if (m_buffer != NULL) { delete[] m_buffer; m_buffer = NULL; }
}
//+------------------------------------------------------------------+
//| 悟牮桢 羿殡?潆 黩屙?                                        |
//| dwAccess       -GENERIC_READ 桦?GENERIC_WRITE                   |
//| dwCreationFlags-CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS        |
//+------------------------------------------------------------------+
bool CStringFile::Open(LPCTSTR lpFileName, const DWORD dwAccess, const DWORD dwCreationFlags)
{
	//--- 玎牮铄?磬 怦觇?耠篦嚅 镳邃簌栝 羿殡
	Close();
	//--- 镳钼屦觇
	if (lpFileName != NULL)
	{
		//--- 耦玟噤桁 羿殡
		m_file = CreateFile(lpFileName, dwAccess, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, dwCreationFlags, FILE_ATTRIBUTE_NORMAL, NULL);
		//--- 铒疱溴腓?疣珈屦 羿殡?(礤 犷朦 4Gb)
		if (m_file != INVALID_HANDLE_VALUE) m_file_size = GetFileSize(m_file, NULL);
	}
	//--- 忮痦屐 疱珞朦蜞?
	return(m_file != INVALID_HANDLE_VALUE);
}
//+------------------------------------------------------------------+
//| 青镨顸 狍翦疣 箨噻囗眍?潆桧??羿殡                             |
//+------------------------------------------------------------------+
int CStringFile::Read(void *buffer, const DWORD length)
{
	DWORD readed = 0;
	//--- 镳钼屦觇
	if (m_file == INVALID_HANDLE_VALUE || buffer == NULL || length < 1) return(0);
	//--- 聍栩噱??忮痦屐 疱珞朦蜞?
	if (ReadFile(m_file, buffer, length, &readed, NULL) == 0) readed = 0;
	//--- 忮痦屐 觐?忸 聍栩囗睇?徉轵
	return(readed);
}
//+------------------------------------------------------------------+
//| 昨屙桢 狍翦疣 箨噻囗眍?潆桧?桤 羿殡?                          |
//+------------------------------------------------------------------+
int CStringFile::Write(const void *buffer, const DWORD length)
{
	DWORD written = 0;
	//--- 镳钼屦觇
	if (m_file == INVALID_HANDLE_VALUE || buffer == NULL || length < 1) return(0);
	//--- 玎镨?溧眄
	if (WriteFile(m_file, buffer, length, &written, NULL) == 0) written = 0;
	//--- 忮痦屐 觐?忸 玎镨襦眄 徉轵
	return(written);
}
//+------------------------------------------------------------------+
//| 蔓耱噔桁? 磬 磬鬣腩 羿殡?                                      |
//+------------------------------------------------------------------+
void CStringFile::Reset()
{
	//--- 襻痤耔?聍弪麒觇
	m_buffer_index = 0;
	m_buffer_readed = 0;
	m_buffer_line = 0;
	//--- 恹耱噔桁? 磬 磬鬣腩 羿殡?
	if (m_file != INVALID_HANDLE_VALUE) SetFilePointer(m_file, 0, NULL, FILE_BEGIN);
}
//+------------------------------------------------------------------+
//| 青镱腠屐 耱痤牦 ?忸玮疣屐 眍戾?耱痤觇. 0-铠栳赅           |
//+------------------------------------------------------------------+
int CStringFile::GetNextLine(char *line, const int maxsize)
{
	char  *currsym = line, *lastsym = line + maxsize;
	BYTE  *curpos = m_buffer + m_buffer_index;
	//--- 镳钼屦觇
	if (line == NULL || m_file == INVALID_HANDLE_VALUE || m_buffer == NULL) return(0);
	//--- 牮篁桁? ?鲨觌?
	for (;;)
	{
		//--- 镥疴? 耱痤赅 桦?镳铟栩嚯?忮顸 狍翦?
		if (m_buffer_line == 0 || m_buffer_index == m_buffer_readed)
		{
			//--- 玎眢?屐 聍弪麒觇
			m_buffer_index = 0;
			m_buffer_readed = 0;
			//--- 麒蜞屐 ?狍翦?
			if (::ReadFile(m_file, m_buffer, m_buffer_size, (DWORD*)&m_buffer_readed, NULL) == 0)
			{
				Close();
				return(0);
			}
			//--- 聍栩嚯?0 徉轵? 觐礤?羿殡?
			if (m_buffer_readed < 1) { *currsym = 0; return(currsym != line ? m_buffer_line : 0); }
			curpos = m_buffer;
		}
		//--- 囗嚯桤桊箦?狍翦?
		while (m_buffer_index < m_buffer_readed)
		{
			//--- 漕?漕 觐眦?
			if (currsym >= lastsym) { *currsym = 0; return(m_buffer_line); }
			//--- 镳钹磬腓玷痼屐 耔焘铍 (磬?觐礤?耱痤觇)
			if (*curpos == '\n')
			{
				//--- 猁?腓 镥疱?桁 忸玮疣?赅疱蜿?
				if (currsym > line && currsym[-1] == '\r') currsym--; // 猁?恹蜩疣屐 邈?
				*currsym = 0;
				//--- 忸玮疣屐 眍戾?耱痤觇
				m_buffer_line++;
				m_buffer_index++;
				return(m_buffer_line);
			}
			//--- 钺睇?耔焘铍-觐镨痼屐 邈?
			*currsym++ = *curpos++; m_buffer_index++;
		}
	}
	//--- ?礤忸珈铈眍...
	return(0);
}
//+------------------------------------------------------------------+
