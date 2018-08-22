// MyServerPlugin.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <regex>
#define ENABLE "YES"
//---
PluginInfo			ExtPluginInfo = { "ReasonModify", 101, "TigerWit Corp.", { 0 } };
CServerInterface   *ExtServer = NULL;

vector<string>      m_ABook;
CSync				m_ABook_mutex;
vector<string>		m_Symbols;
CSync				m_Symbols_mutex;
string				m_Enable;
CSync				m_Enable_mutex;

bool				debug = true;

int CloseOrderModifyReason(const int order, const int type);
int UserInfoGet(const int nLogin, UserInfo *pUserInfo);
vector<string> split(string str, string pattern);
bool GetGroupType(const string group);
bool JudgeSymbol(const string symbol);
int CharArrayFind(char* source, char* target);
//+------------------------------------------------------------------+
//| About, must be present always!                                   |
//+------------------------------------------------------------------+
void APIENTRY MtSrvAbout(PluginInfo *info){
	if (info != NULL) memcpy(info, &ExtPluginInfo, sizeof(PluginInfo));
}

//+------------------------------------------------------------------+
//| Set server interface point                                       |
//+------------------------------------------------------------------+
int APIENTRY MtSrvStartup(CServerInterface *server){
	//--- check version
	if (server == NULL)                        return(FALSE);
	if (server->Version() != ServerApiVersion) return(FALSE);
	//--- save server interface link
	ExtServer = server;
	PluginCfg cfg = { 0 };
	string strLogInfo = "ReasonModify, install success";
	ExtServer->LogsOut(CmdWarn, "ReasonModify", strLogInfo.c_str());
	ExtConfig.Get("A_BOOK", &cfg);
	ExtServer->LogsOut(CmdWarn, "ReasonModify A_BOOK:", cfg.value); //just for debug to use it 
	m_ABook = split(cfg.value, ";");
	memset(&cfg, 0, sizeof(cfg));
	ExtConfig.Get("SYMBOLS", &cfg);
	ExtServer->LogsOut(CmdWarn, "ReasonModify SYMBOLS:", cfg.value);//just for debug to use it 
	m_Symbols = split(cfg.value, ";");
	memset(&cfg, 0, sizeof(cfg));
	ExtConfig.Get("ENABLE", &cfg);
	ExtServer->LogsOut(CmdWarn, "ReasonModify ENABLE:", cfg.value);//just for debug to use it 
	m_Enable = cfg.value;

	return(TRUE);
}

int APIENTRY MtSrvTradeTransaction(TradeTransInfo* trans, const UserInfo *user, int *request_id)
{
	m_Enable_mutex.Lock();
	if (m_Enable != ENABLE){
		m_Enable_mutex.Unlock();
		return RET_OK;
	}
	m_Enable_mutex.Unlock();

	if (trans->cmd > OP_SELL)
		return RET_OK;

	if (!JudgeSymbol(trans->symbol))
	{
		return RET_OK;
	}

	if (debug)
	{
		char strFormatBuff[255] = { 0 };
		sprintf(strFormatBuff, "[debug-trans] order:%d, order_by:%d, login:%d, type:%d, symbol:%s, comment:%s", 
			trans->order, trans->orderby, user->login, trans->type, trans->symbol, trans->comment);
		ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
	}

	if (trans->type == TT_ORDER_MK_CLOSE || trans->type == TT_BR_ORDER_CLOSE)
	{	//reason modify for close order here
		if (!CloseOrderModifyReason(trans->order, trans->type))
		{
			string strLogInfo = "ReasonModify, Error";
			ExtServer->LogsOut(CmdWarn, "ReasonModify", strLogInfo.c_str());
		}
	}
	else if (trans->type == TT_ORDER_MK_OPEN && trans->orderby == 0)	//MT4 Terminal
	{
		//reason modify for open order in MtSrvTradesAdd
		char * mo1 = "[MO]";
		char * mo2 = "Manager Open";

		if ((CharArrayFind(trans->comment, mo2) > -1) || (CharArrayFind(trans->comment, mo1) > -1))
		{
			trans->comment[0] = '\0';

			char strFormatBuff[255] = { 0 };
			sprintf(strFormatBuff, "[Open Before] order:%d, order_by:%d, login:%d, comment:%s",
				trans->order, trans->orderby, user->login, trans->comment);
			ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
		}
	}

	return RET_OK;
}

void APIENTRY  MtSrvTradesAdd(TradeRecord* trade, const UserInfo* user, const ConSymbol* symbol)
{
	m_Enable_mutex.Lock();
	if (m_Enable != ENABLE){
		m_Enable_mutex.Unlock();
		return;
	}
	m_Enable_mutex.Unlock();

	if (trade->cmd > OP_SELL)
		return;

	if (!JudgeSymbol(trade->symbol))
	{
		return;
	}

	if (debug)
	{
		char strFormatBuff[255] = { 0 };
		sprintf(strFormatBuff, "[debug-add] order:%d, order_by:%d, login:%d, symbol:%s, reason:%d, comment:%s",
			trade->order, trade->login, user->login, trade->symbol, trade->reason, trade->comment);
		ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
	}

	char * mo1 = "[MO]";
	char * mo2 = "Manager Open";

	if ((CharArrayFind(trade->comment, mo2) > -1) || (CharArrayFind(trade->comment, mo1) > -1))
	{
		int reason = trade->reason;

		if (reason == TR_REASON_DEALER || reason == TR_REASON_API)
			return;

		trade->reason = TR_REASON_DEALER;

		UserInfo info = { 0 };
		if (UserInfoGet(trade->login, &info) == FALSE)
			return;

		if (ExtServer->OrdersUpdate(trade, &info, UPDATE_NORMAL) == FALSE)
			return;

		char strFormatBuff[255] = { 0 };
		sprintf(strFormatBuff, "[Open After] order:%d, login:%d, reason_old:%d, reason_new:%d, comment:%s", 
			trade->order, trade->login, reason, trade->reason, trade->comment);
		ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
	}
}

int CloseOrderModifyReason(const int order, const int type)
{
	TradeRecord oldtrade = { 0 }, trade = { 0 };
	UserInfo    info = { 0 };
	//--- checks
	if (order <= 0 || ExtServer == NULL) return(FALSE);
	//--- get order
	if (ExtServer->OrdersGet(order, &oldtrade) == FALSE)
		return(FALSE); // error
	//--- check
	if (oldtrade.cmd > OP_SELL)
		return(FALSE); // order already activated
	//--- check
	if (oldtrade.close_time != 0)
		return(FALSE); // order already deleted
	//--- prepare new trade state of order
	memcpy(&trade, &oldtrade, sizeof(TradeRecord));
	//--- change reason
	bool needChange = false;
	char strFormatBuff[255] = { 0 };
	sprintf(strFormatBuff, "[Close Before] order:%d, login:%d, reason_old:%d, type:%d, comment:%s", 
		order, oldtrade.login, oldtrade.reason, type, oldtrade.comment);
	ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
	if (type == TT_ORDER_MK_CLOSE && (oldtrade.reason == TR_REASON_DEALER || oldtrade.reason == TR_REASON_API))
	{
		//close - open:manager, close:client - change to close
		trade.reason = TR_REASON_CLIENT;
		needChange = true;
	}
	else if (type == TT_BR_ORDER_CLOSE && (oldtrade.reason != TR_REASON_DEALER && oldtrade.reason != TR_REASON_API))
	{
		//close - open:client, close:manager - change to close
		trade.reason = TR_REASON_DEALER;
		needChange = true;
	}

	if (needChange)
	{
		//--- get user info
		if (UserInfoGet(oldtrade.login, &info) == FALSE)
			return(FALSE); // error
		//--- get group type if it is abook need to process the order. add by wzp 2018-06-04
		//if (!GetGroupType(info.group)){
		//	return(TRUE);
		//}
		//--- update order
		if (ExtServer->OrdersUpdate(&trade, &info, UPDATE_NORMAL) == FALSE)
			return(FALSE); // error
		//--- journal
		char strFormatBuff[255] = { 0 };
		sprintf(strFormatBuff, "[Close After] order:%d, login:%d, reason_old:%d, reason_new:%d, comment:%s", 
			order, oldtrade.login, oldtrade.reason, trade.reason, trade.comment);
		ExtServer->LogsOut(CmdWarn, "ReasonModify", strFormatBuff);
	}

	return(TRUE);
}

//+------------------------------------------------------------------+
//| Standard configuration functions                                 |
//+------------------------------------------------------------------+
int APIENTRY MtSrvPluginCfgAdd(const PluginCfg *cfg){
	int res = ExtConfig.Add(cfg);
	return(res);
}

int APIENTRY MtSrvPluginCfgSet(const PluginCfg *values, const int total){
	string tmp;
	if (NULL == values){
		return(FALSE);
	}

	//ExtServer->LogsOut(CmdWarn, "ReasonModify values", values->name);

	for (int i = 0; i < total; i++){
		tmp = values[i].name;

		if ("A_BOOK" == tmp){
			m_ABook_mutex.Lock();
			m_ABook.clear();
			m_ABook = split(values[i].value, ";");
			ExtServer->LogsOut(CmdWarn, "ReasonModify m_ABook", values[i].value);
			m_ABook_mutex.Unlock();
		}
		else if ("SYMBOLS" == tmp){
			m_Symbols_mutex.Lock();
			m_Symbols.clear();
			m_Symbols = split(values[i].value, ";");
			ExtServer->LogsOut(CmdWarn, "ReasonModify m_Symbols", values[i].value);
			m_Symbols_mutex.Unlock();
		}
		else if ("ENABLE" == tmp){
			m_Enable_mutex.Lock();
			m_Enable = values[i].value;
			ExtServer->LogsOut(CmdWarn, "ReasonModify ENABLE", m_Enable.c_str());
			m_Enable_mutex.Unlock();
		}
	}

	//ExtServer->LogsOut(CmdWarn, "ReasonModify", "MtSrvPluginCfgSet");
	int res = ExtConfig.Set(values, total);
	return(res);
}

//int APIENTRY MtSrvPluginCfgDelete(LPCSTR name){
//	int res = ExtConfig.Delete(name);
//	return(res);
//}

int APIENTRY MtSrvPluginCfgGet(LPCSTR name, PluginCfg *cfg)              { return ExtConfig.Get(name, cfg); }

int APIENTRY MtSrvPluginCfgNext(const int index, PluginCfg *cfg)         { return ExtConfig.Next(index, cfg); }

int APIENTRY MtSrvPluginCfgTotal()                                      { return ExtConfig.Total(); }


int UserInfoGet(const int nLogin, UserInfo *pUserInfo)
{
	if (nLogin < 1 || pUserInfo == NULL || ExtServer == NULL)
	{
		ExtServer->LogsOut(CmdErr, "UserInfoGet", "parameter error");
		return FALSE;
	}

	UserRecord mUserRecord = { 0 };
	ZeroMemory(pUserInfo, sizeof(UserInfo));

	if (ExtServer->ClientsUserInfo(nLogin, &mUserRecord) == FALSE)
	{
		ExtServer->LogsOut(CmdErr, "UserInfoGet", "login dismatch");
		return FALSE;
	}

	//fill login
	pUserInfo->login = mUserRecord.login;

	//fill group
	COPY_STR(pUserInfo->group, mUserRecord.group);

	//fill permissions
	pUserInfo->enable = mUserRecord.enable;
	pUserInfo->enable_read_only = mUserRecord.enable_read_only;

	//fill trade data
	pUserInfo->leverage = mUserRecord.leverage;
	pUserInfo->agent_account = mUserRecord.agent_account;
	pUserInfo->credit = mUserRecord.credit;
	pUserInfo->balance = mUserRecord.balance;
	pUserInfo->prevbalance = mUserRecord.prevbalance;

	return TRUE;
}

vector<string> split(string str, string pattern)
{
	vector<string> ret;
	if (pattern.empty()) return ret;
	size_t start = 0, index = str.find_first_of(pattern, 0);
	while (index != str.npos)
	{
		if (start != index)
			ret.push_back(str.substr(start, index - start));
		start = index + 1;
		index = str.find_first_of(pattern, start);
	}

	if (!str.substr(start).empty())
		ret.push_back(str.substr(start));
	return ret;
}

//bool JudgeSymbol(const string symbol){
//	vector<string>::iterator ita;
//	string tmp;
//	m_Symbols_mutex.Lock();
//	if (0 == m_Symbols.size())//if no set symbols value we need process all symbols.
//		return true;
//	for (ita = m_Symbols.begin(); ita != m_Symbols.end(); ita++){
//		tmp = *ita;
//		if (*ita == symbol){
//			//ExtServer->LogsOut(CmdWarn, "ReasonModify", symbol.c_str());
//			m_Symbols_mutex.Unlock();
//			return true;
//		}
//	}
//	m_Symbols_mutex.Unlock();
//
//	return false;
//}
//add by wzp 2018-08-22
bool JudgeSymbol(const string symbol){
	vector<string>::iterator ita;
	string tmp;
	bool InFlag = true, result = false;
	int index_start = 0, index_end = 0, index_ex = 0;

	for (ita = m_Symbols.begin(); ita != m_Symbols.end(); ita++){
		tmp = *ita;
		index_ex = tmp.find_first_of("!");

		if (0 == index_ex){
			InFlag = false;
		}

		index_start = tmp.find_first_of("*");
		index_end = tmp.find_last_of("*");

		if (index_start < index_end){
			tmp = "[\\s\\S]" + tmp.substr(index_start, index_end - index_start) + "[\\s\\S]*";
		} else if (index_start == index_end){
			if (-1 == index_start){//no *symbol

			} else if (index_start > 1){
				tmp = tmp.substr(0, index_end) + "[\\s\\S]*";
			} else if (InFlag) {
				tmp = "[\\s\\S]" + tmp.substr(index_start, tmp.length());
			}
		}
	//	ExtServer->LogsOut(CmdWarn, "JudgeSymbol", tmp.c_str());
		if (!InFlag){
			tmp = "!" + tmp;
		}

		regex r(tmp);

		if (std::regex_match(symbol, r)){
			result = true;
			return result;
		}
	}

	return result;
}

bool GetGroupType(const string group){
	//int flag = 0;
	vector<string>::iterator ita;
	string tmp;
	m_ABook_mutex.Lock();
	if (0 == m_ABook.size()){
		//if no set abook value we need process all group.
		m_ABook_mutex.Unlock();
		return true;
	}
	for (ita = m_ABook.begin(); ita != m_ABook.end(); ita++){
		tmp = *ita;
		if (string::npos != group.find(*ita)){
			//ExtServer->LogsOut(CmdWarn, "ReasonModify", group.c_str());
			m_ABook_mutex.Unlock();
			return true;
		}
	}

	m_ABook_mutex.Unlock();
	return false;
}

int CharArrayFind(char* source, char* target)	//source是主串，target是子串
{
	int i, j;
	int s_len = strlen(source);
	int t_len = strlen(target);
	if (t_len > s_len)
	{
		return -1;
	}
	for (i = 0; i <= s_len - t_len; i++)
	{
		j = 0;
		int flag = 1;
		if (source[i] == target[j])
		{
			int k, p = i;
			for (k = 0; k < t_len; k++)
			{
				if (source[p] == target[j])
				{
					p++;
					j++;
					continue;

				}
				else
				{
					flag = 0;
					break;
				}
			}
		}
		else
		{
			continue;
		}
		if (flag == 1)
		{
			return i;
		}
	}
	return -1;
}