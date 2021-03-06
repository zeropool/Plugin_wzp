//+------------------------------------------------------------------+
//|                                            MetaTrader Server API |
//|                   Copyright 2001-2014, MetaQuotes Software Corp. |
//|                                        http://www.metaquotes.net |
//+------------------------------------------------------------------+
#include "stdafx.h"
#include "Configuration.h"
#include "stringfile.h"

CConfiguration ExtConfig;
//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
CConfiguration::CConfiguration() : m_cfg(NULL), m_cfg_total(0), m_cfg_max(0)
{
	m_filename[0] = 0;
}
//+------------------------------------------------------------------+
//|                                                                  |
//+------------------------------------------------------------------+
CConfiguration::~CConfiguration()
{
	m_sync.Lock();
	//---
	if (m_cfg != NULL) { delete[] m_cfg; m_cfg = NULL; }
	m_cfg_total = m_cfg_max = 0;
	//---
	m_sync.Unlock();
}
//+------------------------------------------------------------------+
//| 帖 蝾朦觐 麒蜞屐 觐眙桡箴圉棹, 眍 龛麇泐 礤 戾?屐 ?礤?        |
//+------------------------------------------------------------------+
void CConfiguration::Load(LPCSTR filename)
{
	char        tmp[MAX_PATH], *cp, *start;
	CStringFile in;
	PluginCfg   cfg, *buf;
	//--- 镳钼屦赅
	if (filename == NULL) return;
	//--- 耦躔囗桁 桁 觐眙桡箴圉桀眄钽?羿殡?
	m_sync.Lock();
	COPY_STR(m_filename, filename);
	//--- 铗牮铄?羿殡
	if (in.Open(m_filename, GENERIC_READ, OPEN_EXISTING))
	{
		while (in.GetNextLine(tmp, sizeof(tmp) - 1) > 0)
		{
			if (tmp[0] == ';') continue;
			//--- 镳铒篑蜩?镳钺咫?
			start = tmp; while (*start == ' ') start++;
			if ((cp = strchr(start, '=')) == NULL) continue;
			*cp = 0;
			//--- 玎眢腓? 徨疱?桁 镟疣戾蝠?
			ZeroMemory(&cfg, sizeof(cfg));
			COPY_STR(cfg.name, start);
			//--- 铒螯 镳铒篑蜩?镳钺咫?
			cp++; while (*cp == ' ') cp++;
			COPY_STR(cfg.value, cp);
			//--- 镳钼屦屐
			if (cfg.name[0] == 0 || cfg.value[0] == 0) continue;
			//--- 漕徉怆屐
			if (m_cfg == NULL || m_cfg_total >= m_cfg_max) // 戾耱?羼螯?
			{
				//--- 镥疱恹溴腓?眍恹?狍翦?
				if ((buf = new PluginCfg[m_cfg_total + 64]) == NULL) break;
				//--- 耜铒桊箦?铖蜞蜿?桤 耱囵钽?
				if (m_cfg != NULL)
				{
					if (m_cfg_total > 0) memcpy(buf, m_cfg, sizeof(PluginCfg)*m_cfg_total);
					delete[] m_cfg;
				}
				//--- 镱潇屙桁 狍翦?
				m_cfg = buf;
				m_cfg_max = m_cfg_total + 64;
			}
			//--- 觐镨痼屐
			memcpy(&m_cfg[m_cfg_total++], &cfg, sizeof(PluginCfg));
		}
		//--- 玎牮噱祚
		in.Close();
	}
	//--- 蝈镥瘘 忸琰旄??铗耦痱桊箦?镱 桁屙?(黩钺?桉赅螯 猁耱痤)
	if (m_cfg != NULL && m_cfg_total > 0) qsort(m_cfg, m_cfg_total, sizeof(PluginCfg), SortByName);
	m_sync.Unlock();
}
//+------------------------------------------------------------------+
//| 厌痤?觐眙桡钼 磬 滂耜                                           |
//+------------------------------------------------------------------+
void CConfiguration::Save(void)
{
	CStringFile out;
	char        tmp[512];
	//--- 玎镨?怦?磬 滂耜
	m_sync.Lock();
	if (m_filename[0] != 0)
		if (out.Open(m_filename, GENERIC_WRITE, CREATE_ALWAYS))
		{
			if (m_cfg != NULL)
				for (int i = 0; i < m_cfg_total; i++)
				{
					_snprintf(tmp, sizeof(tmp) - 1, "%s=%s\n", m_cfg[i].name, m_cfg[i].value);
					if (out.Write(tmp, strlen(tmp)) < 1) break;
				}
			//--- 玎牮铄?羿殡
			out.Close();
		}
	m_sync.Unlock();
	//---
}
//+------------------------------------------------------------------+
//| 湾犭铌桊箦禧?镱桉?镱 桁屙?                                    |
//+------------------------------------------------------------------+
PluginCfg* CConfiguration::Search(LPCSTR name)
{
	PluginCfg *config = NULL;
	//---
	if (m_cfg != NULL && m_cfg_total > 0)
		config = (PluginCfg*)bsearch(name, m_cfg, m_cfg_total, sizeof(PluginCfg), SearchByName);
	//---
	return(config);
}
//+------------------------------------------------------------------+
//| 念徉怆屙桢/祛滂翳赅鲨 镫嚆桧?                                  |
//+------------------------------------------------------------------+
int CConfiguration::Add(const PluginCfg *cfg)
{
	PluginCfg *config, *buf;
	//--- 镳钼屦觇
	if (cfg == NULL || cfg->name[0] == 0) return(FALSE);
	//---
	m_sync.Lock();
	if ((config = Search(cfg->name)) != NULL) memcpy(config, cfg, sizeof(PluginCfg));
	else
	{
		//--- 戾耱?羼螯?
		if (m_cfg == NULL || m_cfg_total >= m_cfg_max)
		{
			//--- 恹溴腓?戾耱?
			if ((buf = new PluginCfg[m_cfg_total + 64]) == NULL) { m_sync.Unlock(); return(FALSE); }
			//--- 耜铒桊箦?铖蜞蜿?桤 耱囵钽?狍翦疣
			if (m_cfg != NULL)
			{
				if (m_cfg_total > 0) memcpy(buf, m_cfg, sizeof(PluginCfg)*m_cfg_total);
				delete[] m_cfg;
			}
			//--- 玎戾龛?狍翦?
			m_cfg = buf;
			m_cfg_max = m_cfg_total + 64;
		}
		//--- 漕徉怆屐 ?觐礤?
		memcpy(&m_cfg[m_cfg_total++], cfg, sizeof(PluginCfg));
		//--- 铗耦痱桊箦祚
		qsort(m_cfg, m_cfg_total, sizeof(PluginCfg), SortByName);
	}
	m_sync.Unlock();
	//--- 耦躔囗桁?, 镥疱玎麒蜞屐?
	Save();
	//--- 恹躅滂?
	return(TRUE);
}
//+------------------------------------------------------------------+
//| 蔓耱噔?屐 磬犷?磬耱痤尻                                        |
//+------------------------------------------------------------------+
int CConfiguration::Set(const PluginCfg *values, const int total)
{
	//--- 镳钼屦觇
	if (total < 0) return(FALSE);
	//---
	m_sync.Lock();
	if (values != NULL && total>0)
	{
		//--- 戾耱?羼螯?
		if (m_cfg == NULL || total >= m_cfg_max)
		{
			//--- 箐嚯桁 耱囵 ?恹溴腓?眍恹?狍翦?
			if (m_cfg != NULL) delete[] m_cfg;
			if ((m_cfg = new PluginCfg[total + 64]) == NULL)
			{
				m_cfg_max = m_cfg_total = 0;
				m_sync.Unlock();
				return(FALSE);
			}
			//--- 恹耱噔桁 眍恹?镳邃咫
			m_cfg_max = total + 64;
		}
		//--- 耜铒桊箦?怦屐 耜铒铎
		memcpy(m_cfg, values, sizeof(PluginCfg)*total);
	}
	//--- 恹耱噔桁 钺?觐腓麇耱忸 ?铗耦痱桊箦祚
	m_cfg_total = total;
	if (m_cfg != NULL && m_cfg_total > 0) qsort(m_cfg, m_cfg_total, sizeof(PluginCfg), SortByName);
	m_sync.Unlock();
	//--- 耦躔囗桁?
	Save();
	return(TRUE);
}
//+------------------------------------------------------------------+
//| 腮屐 觐眙桡 镱 桁屙?                                            |
//+------------------------------------------------------------------+
int CConfiguration::Get(LPCSTR name, PluginCfg *cfg)
{
	PluginCfg *config = NULL;
	//--- 镳钼屦觇
	if (name != NULL && cfg != NULL)
	{
		m_sync.Lock();
		if ((config = Search(name)) != NULL) memcpy(cfg, config, sizeof(PluginCfg));
		m_sync.Unlock();
	}
	//--- 忮痦屐 疱珞朦蜞?
	return(config != NULL);
}
//+------------------------------------------------------------------+
//| 腮屐 觐眙桡                                                      |
//+------------------------------------------------------------------+
int CConfiguration::Next(const int index, PluginCfg *cfg)
{
	//--- 镳钼屦觇
	if (cfg != NULL && index >= 0)
	{
		m_sync.Lock();
		if (m_cfg != NULL && index < m_cfg_total)
		{
			memcpy(cfg, &m_cfg[index], sizeof(PluginCfg));
			m_sync.Unlock();
			return(TRUE);
		}
		m_sync.Unlock();
	}
	//--- 礤箐圜?
	return(FALSE);
}
//+------------------------------------------------------------------+
//| 愉嚯屐 觐眙桡                                                   |
//+------------------------------------------------------------------+
int CConfiguration::Delete(LPCSTR name)
{
	PluginCfg *config = NULL;
	//--- 镳钼屦觇
	if (name != NULL)
	{
		m_sync.Lock();
		if ((config = Search(name)) != NULL)
		{
			int index = config - m_cfg;
			if ((index + 1) < m_cfg_total) memmove(config, config + 1, sizeof(PluginCfg)*(m_cfg_total - index - 1));
			m_cfg_total--;
		}
		//--- 铗耦痱桊箦祚
		if (m_cfg != NULL && m_cfg_total > 0) qsort(m_cfg, m_cfg_total, sizeof(PluginCfg), SortByName);
		m_sync.Unlock();
	}
	//--- 忮痦屐 疱珞朦蜞?
	return(config != NULL);
}
//+------------------------------------------------------------------+
//| 杨痱桊钼赅 镱 桁屙?                                             |
//+------------------------------------------------------------------+
int CConfiguration::SortByName(const void *left, const void *right)
{
	return strcmp(((PluginCfg*)left)->name, ((PluginCfg*)right)->name);
}
//+------------------------------------------------------------------+
//| 项桉?镱 桁屙?                                                  |
//+------------------------------------------------------------------+
int CConfiguration::SearchByName(const void *left, const void *right)
{
	return strcmp((char*)left, ((PluginCfg*)right)->name);
}
//+------------------------------------------------------------------+
//| 腮屐 觐眙桡 镱 桁屙?                                            |
//+------------------------------------------------------------------+
int CConfiguration::GetInteger(LPCSTR name, int *value, LPCSTR defvalue)
{
	PluginCfg *config = NULL;
	//--- 镳钼屦觇
	if (name != NULL && value != NULL)
	{
		m_sync.Lock();
		if ((config = Search(name)) != NULL) *value = atoi(config->value);
		else
			if (defvalue != NULL)
			{
				m_sync.Unlock();
				//--- 耦玟噤桁 眍怏?玎镨顸
				PluginCfg cfg = { 0 };
				COPY_STR(cfg.name, name);
				COPY_STR(cfg.value, defvalue);
				Add(&cfg);
				//--- 恹耱噔桁 珥圜屙桢 镱 箪铍鬣龛??忮痦屐?
				*value = atoi(cfg.value);
				return(TRUE);
			}
		m_sync.Unlock();
	}
	//--- 忮痦屐 疱珞朦蜞?
	return(config != NULL);
}
//+------------------------------------------------------------------+
//| 腮屐 觐眙桡 镱 桁屙?                                            |
//+------------------------------------------------------------------+
int CConfiguration::GetDouble(LPCSTR name, double *value, LPCSTR defvalue)
{
	PluginCfg *config = NULL;
	//--- 镳钼屦觇
	if (name != NULL && value != NULL)
	{
		m_sync.Lock();
		if ((config = Search(name)) != NULL) *value = atof(config->value);
		else
			if (defvalue != NULL)
			{
				m_sync.Unlock();
				//--- 耦玟噤桁 眍怏?玎镨顸
				PluginCfg cfg = { 0 };
				COPY_STR(cfg.name, name);
				COPY_STR(cfg.value, defvalue);
				Add(&cfg);
				//--- 恹耱噔桁 珥圜屙桢 镱 箪铍鬣龛??忮痦屐?
				*value = atof(cfg.value);
				return(TRUE);
			}
		m_sync.Unlock();
	}
	//--- 忮痦屐 疱珞朦蜞?
	return(config != NULL);
}
//+------------------------------------------------------------------+
