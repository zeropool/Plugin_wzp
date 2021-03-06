//+------------------------------------------------------------------+
//|                                            MetaTrader Server API |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#pragma once

//+------------------------------------------------------------------+
//| 孰囫?镱耱痤觐忸泐 黩屙? 羿殡?                                 |
//+------------------------------------------------------------------+
class CStringFile
{
private:
	HANDLE            m_file;                 // 蹂礓?羿殡?
	DWORD             m_file_size;            // 疣珈屦 羿殡?
	BYTE             *m_buffer;               // 狍翦?潆 黩屙?
	int               m_buffer_size;          // 邈?疣珈屦
	int               m_buffer_index;         // 蝈牦 镱玷鲨 镟瘃桧汔
	int               m_buffer_readed;        // 疣珈屦 聍栩囗眍泐 ?镟?螯 狍翦疣
	int               m_buffer_line;          // 聍弪麒?耱痤??羿殡?

public:
	CStringFile(const int nBufSize = 65536);
	~CStringFile();
	//---
	bool              Open(LPCTSTR lpFileName, const DWORD dwAccess, const DWORD dwCreationFlags);
	inline void       Close() { if (m_file != INVALID_HANDLE_VALUE) { CloseHandle(m_file); m_file = INVALID_HANDLE_VALUE; } m_file_size = 0; }
	inline DWORD      Size() const { return(m_file_size); }
	int               Read(void  *buffer, const DWORD length);
	int               Write(const void *buffer, const DWORD length);
	void              Reset();
	int               GetNextLine(char *line, const int maxsize);
};
//+------------------------------------------------------------------+
