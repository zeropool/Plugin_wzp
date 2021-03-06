//+------------------------------------------------------------------+
//|                                            MetaTrader Server API |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#pragma once

//+------------------------------------------------------------------+
//| Simple synchronizer                                              |
//+------------------------------------------------------------------+
class CSync
{
private:
	CRITICAL_SECTION  m_cs;

public:
	CSync()  { ZeroMemory(&m_cs, sizeof(m_cs)); InitializeCriticalSection(&m_cs); }
	~CSync()  { DeleteCriticalSection(&m_cs);   ZeroMemory(&m_cs, sizeof(m_cs)); }
	inline void       Lock()   { EnterCriticalSection(&m_cs); }
	inline void       Unlock() { LeaveCriticalSection(&m_cs); }
};
//+------------------------------------------------------------------+
//| Simple configuration                                             |
//+------------------------------------------------------------------+
class CConfiguration
{
private:
	CSync             m_sync;                 // 耔眭痤龛玎蝾?
	char              m_filename[MAX_PATH];   // 桁 羿殡?觐眙桡箴圉梃
	PluginCfg        *m_cfg;                  // 爨耨桠 玎镨皴?
	int               m_cfg_total;            // 钺?觐腓麇耱忸 玎镨皴?
	int               m_cfg_max;              // 爨犟桁嚯铄 觐腓麇耱忸 玎镨皴?

public:
	CConfiguration();
	~CConfiguration();
	//--- 桧桷栲腓玎鲨 徉琨 (黩屙桢 觐眙桡 羿殡?
	void              Load(LPCSTR filename);
	//--- 漕耱箫 ?玎镨??
	int               Add(const PluginCfg* cfg);
	int               Set(const PluginCfg *values, const int total);
	int               Get(LPCSTR name, PluginCfg* cfg);
	int               Next(const int index, PluginCfg* cfg);
	int               Delete(LPCSTR name);
	inline int        Total(void) { m_sync.Lock(); int total = m_cfg_total; m_sync.Unlock(); return(total); }

	int               GetInteger(LPCSTR name, int *value, LPCSTR defvalue = NULL);
	int               GetDouble(LPCSTR name, double *value, LPCSTR defvalue = NULL);

private:
	void              Save(void);
	PluginCfg*        Search(LPCSTR name);
	static int        SortByName(const void *left, const void *right);
	static int        SearchByName(const void *left, const void *right);
};

extern CConfiguration ExtConfig;
//+------------------------------------------------------------------+
