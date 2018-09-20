#pragma once
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <stack>
#include "zmq.hpp"
#include "MT4ManagerAPI.h"
#include "msg_define_new.pb.h"
#include "get_config.h"
#include "log4_cplus.h"
#include "curl/curl.h"
using namespace std;
//define bridge ret message
#define SEND 1
#define REJECT 2
#define RESET 3
#define A_BOOK  1
#define B_BOOK	2
//add by wzp 2018-09-19
#define D_PE "[pe]"
#define D_TP "[tp]"
#define D_SL "[sl]"
#define D_SO "[so]"
#define MANAGER "MANAGER"

#define FX		"FX"
#define METAL	"METAL"
#define ENERGY	"ENERGY"
#define CFD		"CFD"

#define REX_FX		"^(AUD|CAD|CHF|EUR|GBP|NZD|USD)[\\s\\S]*"
#define REX_METAL	"^(XAU|XAG)[\\s\\S]*"
#define REX_ENERGY  "XNGUSD|XBRUSD|XTIUSD"
#define REX_CFD		"AUS200|EUSTX50|GER30|HK50|JPN225|SPA35|NAS100|UK100|US500|US30|USA500|FRA40|D30EUR|U30USD|200AUD|H33HKD|225JPY|F40EUR|E50EUR|100GBP|SPXUSD|NASUSD"
//#pragma comment(lib, "ws2_32.lib")
//#pragma comment(lib, "wldap32.lib")
//#pragma comment(lib, "libcurl.lib")
void __stdcall OnDealingFunc(int code);
void __stdcall OnPumpingFunc(int code, int type, void *data, void *param);
void __stdcall PumpingNotify(int code, int type, void *data, void *param);
double __fastcall NormalizeDouble(const double val, int digits);
size_t req_reply(void *ptr, size_t size, size_t nmemb, void *stream);
DWORD WINAPI TransferMsgFromBridgeToMT4(LPVOID lparamter);
DWORD WINAPI KeepLiveForMT4(LPVOID lparamter);



static int gettimeofday(struct timeval* tv);
static time_t GetUtcCaressing();

static const double ExtDecimalArray[9] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0 };
//ReadConfig("test.cfg", m);
//extern map<string, string> m_config; // config file add by wzp
struct OrderValue{
	int              OrderNum;              // order number
	time_t			 timestrap;             // time strap
	int				 status;				//status 0.doing 1.failed 2.done.
	int				 activation;			
};

struct SymbolsValue{
	int               type;                  // symbol type (symbols group index)
	double            point;                 // symbol point=1/pow(10,digits)
	int               digits;                // floating point digits
};

//add by wzp 2018-07-27
struct SymbolConfigInfo{
	double contract_size;
	double margin_divider;
	double margin_initial;
	string margin_currency;
	string type;
};

//add by wzp 2018-09-11
struct SymbolGroupRegex{
	bool   bReverse;
//	regex  re;
	string rule;
};

//add by wzp 2018-08-01
struct GroupConfigInfo{
	string		 currency;
	ConGroupSec  secgroups[MAX_SEC_GROUPS];//record the spread info.
};


template <class K, class V> struct MutexMap{
	map<K, V> m_queue;
	mutex m_mutex;

	void Add(const K &key, const V &value){
		m_mutex.lock();
		m_queue[key] = value;
		m_mutex.unlock();
	//	ExtLogger.Out("m_queue[key].type:%d,m_queue[key].point:%llf,m_queue[key].digits:%d", m_queue[key].type, m_queue[key].point, m_queue[key].digits);
	};

	bool Get(const K &key, V &value){
		bool flag = false;
		m_mutex.lock();
		if (m_queue.find(key) != m_queue.end()){
			value = m_queue[key];
			flag = true;
		}
		m_mutex.unlock();
		return flag;
	};

	void Delete(const K &key){
		m_mutex.lock();
		if (m_queue.find(key) != m_queue.end()){
			 m_queue.erase(key);
		}
		m_mutex.unlock();
	}

	// add empty() judge for MutexMap add by wzp.
	bool Empty(){
		m_mutex.lock();
		bool flag = m_queue.empty();
		m_mutex.unlock();

		return flag;
	}
};

class DealerService
{
private:
	//MT4 relationship info
	CManagerFactory					m_factory;

	RequestInfo						m_req;
	RequestInfo						m_req_recv;
	TradeRecord						*m_TradeRecord;

	//vector<string>					m_abook;
	//vector<string>					m_bbook;
	//vector<string>					m_symbols;
	
	//Bridge relationship info
	zmq::context_t					m_context;
	zmq::socket_t					m_socket;

	//singleton mode
	static DealerService			    *m_DealerService;
	static string					     m_path;
	static map<string, string>		     m_config;
	MutexMap<string, SymbolsValue>	     m_Symbols;

	MutexMap<int, OrderValue>			 m_Orders;
	MutexMap<string, int>				 m_SplitOrders;
	MutexMap<string, SymbolConfigInfo>   m_SymbolConfigInfo;
	MutexMap<string, GroupConfigInfo>	 m_GroupConfigInfo;

	MutexMap<int, SymbolGroupRegex>		 m_SRegexArray; //add by wzp 2018-9-11
	MutexMap<int, SymbolGroupRegex>		 m_ARegexArray; //add by wzp 2018-9-19
	MutexMap<int, SymbolGroupRegex>		 m_BRegexArray; //add by wzp 2018-9-19
	//lock 
	mutex							m_Dealer_mutex;
	mutex							m_ExtManager_mutex;
	mutex							m_Pump_mutex;
	//manager interface 
	CManagerInterface				*m_ExtManagerPump;
	CManagerInterface				*m_ExtDealer;
	CManagerInterface				*m_ExtManager;
	//bakup manager interface
	CManagerInterface				*m_ExtManager_bak;

	//the pool of manager interface link
	CManagerInterface				*m_pool[2];
public:
	//MT4 relationship info
	void SendWarnMail(const string &title, const string &content);
private:

	CURLcode curl_post_req(const string &url, const string &postParams, string &response);
public:
	static DealerService* GetInstance();
	//get current absolute path.
	static string GetProgramDir();
	//load config file
	static bool LoadConfig();
	void Close(void);
	~DealerService();
	//create mt4 link and create bridge link.
	bool CreateMT4Link();
	bool Mt4DealerReconnection();
	bool Mt4PumpReconnection();
	bool Mt4DirectReconnection();

	bool CreateBridgeLink();
	//init and create thread .
	bool SwitchMode();
	void InitThread();

	//process message upstream and downstream.
	void ProcessMsgUp();
	bool ProcessMsgDown();

	//m_ExtManagerPump update symbols and ask,bid.
	void UpdateSymbols();
	void UpdateAskAndBid();
	//keep the mt4 link live.
	void KeepLiveLinkMT4();
	void PumpProcessTradeOrder(int code, int type, void *data);
	bool CaculateMargin(const TradeRecord &record);
	bool CurrencyConversion(double &margin, const string &group, const SymbolConfigInfo &info);
	bool ConvUnit(double &value, const string &from, const string &to);
	bool StaticSymbolConfigInfo();
	bool StaticGroupConfigInfo();
private:
	//construct 
	DealerService(const string &lib_path, const string abook, const string bbook, const string symbols);
	//send data  
	bool SendDataToBridge(dealer::RequestInfo *send_buf);
	bool SendDataToMT4(const dealer::resp_msg &ret);
	bool DealerSendDataToMT4(const dealer::resp_msg &ret);
	bool DealerSend(const dealer::resp_msg &ret);
	bool DealerReject(const dealer::resp_msg &ret);
	bool DealerReset(const dealer::resp_msg &ret);
	bool PumpSendDataToMT4(const dealer::resp_msg &ret);
	bool PumpSendDataToMT4(const TradeRecord &record);
	void InitSymbolType();
	bool FilterSplitOrder(const dealer::resp_msg *msg);
	bool JudgeRegex(MutexMap<int, SymbolGroupRegex> &regexArray,const string &info);
	int GetGroupType(const string group);
	bool JudgeSymbol(const string symbol);
	//get symbol info
	bool GetSiInfo(const string &symbol, SymbolInfo &si);
	bool InitRegex(vector<string> &argVector, MutexMap<int, SymbolGroupRegex> &regexArray);
	//get Margin info
	string GetMarginInfo(const TradeRecord &record, const string &group);
	string  GetUserGroup(const int login);
	vector< string> split(string str, string pattern);

	//get spread info 
	bool CaculateSpread(const SymbolInfo  &si, RequestInfo &m_req, bool flag);
	bool CaculateSpreadForPump(TradeTransInfo *info, const dealer::resp_msg &msg);
	bool CaculateAskBidAndSpread(RequestInfo &m_req, bool flag = true);
//	bool GetSpreadAndAskBidInfo(const string &symbol, const string &group, SymbolInfo &si, double &spread_diff);
	bool GetSymbolInfo(const string &symbol, SymbolsValue &sv);
	//protocbuf and Mt4 msg type transfer 
	//MT4 msg -> Protoc msg
	bool TransferMsgToProto(dealer::RequestInfo *msg);
	bool TransferRecordToProto(const TradeRecord &record, dealer::RequestInfo *msg, unsigned int  type, string comment);
	//Protoc msg -> MT4 msg
	bool TransferProtoToMsg(const dealer::resp_msg *msg);
	bool TransferProtoToTrade(TradeTransInfo *info, const dealer::resp_msg &msg);
	//get ask and bid with m_ExtManagerPump then backfill data to RequestInfo.

	bool CreateMT4LinkInterface(CManagerInterface ** m_interface, const string &server, const int &login, const string &passwd);
	bool BusinessJudge(RequestInfo *req);

	//connection pool switch
	void SwitchConnection();
	bool PumpProcessManagerOrder(const TradeRecord * trade, dealer::RequestInfo *info, const int type);
	bool PumpProcessOtherOrder(int type, dealer::RequestInfo *info);
	bool MakeNewOrderForClient(const TradeTransInfo &rec,const int &login);

	//process the sl or tp so order repeat send to dealer.
	bool FilterRepeatOrder(const TradeRecord &rec);
	void DeleteOrderRecord();
	void ModifyOrderRecord(const int &OrderNum, const int &status);
};





