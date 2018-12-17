#include "DealerService.h"
#include <direct.h>
#include <time.h>
#include <regex>
#include <sys/timeb.h>
#include <sstream>


using namespace std;

DealerService *DealerService::m_DealerService = NULL;
string DealerService::m_path = "";
map<string, string> DealerService::m_config;
int Count = 0;//record the dealer add by wzp process order 
//map<string, string> m_config; // config file add by wzp

bool DealerService::LoadConfig(){
	m_path = GetProgramDir();

	if (!ReadConfig(m_path+"/"+"Dealer.cfg", m_config)){
		OutputDebugString("Config:load config file failed");
		return false;
	}

	OutputDebugString("Config:load config file suc");
	PrintConfig(m_config);
	return true;

}
string DealerService::GetProgramDir()
{
	TCHAR exeFullPath[MAX_PATH]; // Full path  
	GetModuleFileName(NULL, exeFullPath, MAX_PATH);
	string strPath = __TEXT("");
	strPath = exeFullPath;    // Get full path of the file  
	int pos = strPath.find_last_of(L'\\', strPath.length());
	return strPath.substr(0, pos);  // Return the directory without the file name 
}
//get instance
DealerService* DealerService::GetInstance(){
	if (NULL == m_DealerService){
		curl_global_init(CURL_GLOBAL_ALL); //the main thread init the curl lib.add by wzp 2018-08-05
		
		if (!LoadConfig()){
			OutputDebugString("ERR:LoadConfig(): failed!");
			return NULL;
		}

		//log file name load and start log.
		string tmp = m_path + "/" + m_config["log_file"];
		DealerLog::GetInstance(tmp+".log");
		OutputDebugString(tmp.c_str());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:log_file:" << tmp.c_str());
		cout << tmp << endl;

		//contact dll path.
		tmp = m_path + "\\" + m_config["lib_path"];
		OutputDebugString(tmp.c_str());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:lib_path:" << tmp.c_str());
		m_DealerService = new DealerService(tmp.c_str(), m_config["a_book"], m_config["b_book"], m_config["symbols"]);
	}

	return m_DealerService;
}


void DealerService::SendWarnMail(const string &title, const string &content){

	if (m_config["mail_enable"] == "NO"){
		return;
	}
	
	string param = "email=" + m_config["mail_addr"] + "&" + "title=" + title + "&" + "content=" + m_config["mail_env"] +":"+ content;
	string postResponseStr;

	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "mail param:" << param);
	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "mail url:" << m_config["mail_urls"]);
	
	CURLcode res = curl_post_req(m_config["mail_urls"], param, postResponseStr);
	
	if (res != CURLE_OK){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "curl_easy_perform() failed: " + string(curl_easy_strerror(res)));
	} else{
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "mail response:" << postResponseStr);
	}
	
}

size_t req_reply(void *ptr, size_t size, size_t nmemb, void *stream)
{
	cout << "----->reply" << endl;
	string *str = (string*)stream;
	cout << *str << endl;
	(*str).append((char*)ptr, size*nmemb);
	return size * nmemb;
}

// http POST
CURLcode DealerService::curl_post_req(const string &url, const string &postParams, string &response)
{
	// init curl
	CURL *curl = curl_easy_init();
	// res code
	CURLcode res;
	if (curl)
	{
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "begin curl:");
		// set params
		curl_easy_setopt(curl, CURLOPT_POST, 1); // post req
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // url
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postParams.c_str()); // params
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // if want to use https
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false); // set peer and host verify false
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req_reply);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_HEADER, 1);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		// start req
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "mid curl:");
		res = curl_easy_perform(curl);
	}
	// release curl
	curl_easy_cleanup(curl);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "end curl:");
	return res;
}

vector<string> DealerService::split(string str, string pattern)
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


//Begin use callback function process message add by wzp
bool DealerService::SwitchMode()
{
	
	char tmp[256];
	int res = m_ExtManagerPump->PumpingSwitchEx(OnPumpingFunc, 0, NULL);

	if (RET_OK != res){
		sprintf(tmp, "ERR:PumpingSwitch error ERRCODE:%", m_ExtManagerPump->ErrorDescription(res));
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "PumpingSwitch error ERRCODE:" << m_ExtManagerPump->ErrorDescription(res));
		OutputDebugString(tmp);
		return false;
	}

	int res1 = m_ExtDealer->DealerSwitch(OnDealingFunc, NULL, 0);

	if (RET_OK != res1){
		sprintf(tmp, "ERR:DealerSwitch error ERRCODE:%s", m_ExtDealer->ErrorDescription(res1));
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "ERR:DealerSwitch error ERRCODE:" << m_ExtManagerPump->ErrorDescription(res));
		OutputDebugString(tmp);
		return false;
	}
	
	return true;
}

//construct Dealer Service add by wzp
DealerService::DealerService(const string &lib_path, const string abook, const string bbook, const string symbols) :m_factory(lib_path.c_str()),
m_ExtDealer(NULL), m_ExtManagerPump(NULL), m_ExtManager(NULL), m_ExtManager_bak(NULL), m_TradeRecord(NULL)
	,m_context(1)//, m_socket(m_context, ZMQ_DEALER)
{
	m_socket_dealer = NULL;
	m_socket_pump = NULL;
	vector<string> m_abook = split(abook, ";");
	vector<string> m_bbook = split(bbook, ";");
	vector<string> m_symbols = split(symbols, ";");

	memset(&m_req, 0, sizeof(struct RequestInfo)); 
	memset(&m_req_recv, 0, sizeof(struct RequestInfo));
	memset(m_pool, 0, sizeof(m_pool));

	InitRegex(m_symbols, m_SRegexArray);
	InitRegex(m_abook, m_ARegexArray);
	InitRegex(m_bbook, m_BRegexArray);
}

//void DealerService::InitSymbolType(){
//	m_SymbolType.insert(pair<string, SymbolConfigInfo>("", {}));
//	m_SymbolType.insert(pair<string, SymbolConfigInfo>("", "CFD"));
//	m_SymbolType.insert(pair<string, SymbolConfigInfo>("", "CFD"));
//}

DealerService::~DealerService(){
	OutputDebugString("DealerService::~DealerService()");
}
//Create mt4 link include socket link and login add by wzp
bool DealerService::CreateMT4Link()
{
	if (!m_factory.IsValid()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"m_factory.IsValid() false");
		OutputDebugString("ERR:m_factory.IsValid() false");
		return false;
	}

	//start init windows socket
	if (RET_OK != m_factory.WinsockStartup()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_factory.WinsockStartup() error");
		OutputDebugString("ERR:m_factory.WinsockStartup() error");
		return false;
	}

	//dealer mode connection.
	if (NULL == m_ExtDealer){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer == NULL");
		OutputDebugString("INFO:m_ExtDealer == NULL");
		m_ExtDealer = m_factory.Create(ManAPIVersion);
	}

	//pump mode connection.
	if (NULL == m_ExtManagerPump){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:m_ExtManagerPump == NULL");
		OutputDebugString("INFO:m_ExtManagerPump == NULL");
		m_ExtManagerPump = m_factory.Create(ManAPIVersion);
	}

	// init connection pool
	for (int i = 0; i < 2; i++){
		if (NULL == m_pool[i]){
			LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"INFO:m_ExtManager == NULL");
			OutputDebugString("INFO:m_ExtManager == NULL");
			m_pool[i] = m_factory.Create(ManAPIVersion);
		}
	}
	
	m_ExtManager = m_pool[0];
	m_ExtManager_bak = m_pool[1];

	if ((NULL == m_ExtDealer) || (NULL == m_ExtManagerPump) || NULL == m_pool[0] || NULL == m_pool[1]){
		cout << "(NULL == m_ExtDealer) || (NULL == m_ExtManagerPump) || NULL == m_pool[0] || NULL == m_pool[1])" << endl;
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "(NULL == m_ExtDealer) || (NULL == m_ExtManagerPump) || NULL == m_pool[0] || NULL == m_pool[1])");
		return false;
	}

	CreateMT4LinkInterface(&m_ExtManager, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);
	CreateMT4LinkInterface(&m_ExtManager_bak, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);
	CreateMT4LinkInterface(&m_ExtDealer, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);
	CreateMT4LinkInterface(&m_ExtManagerPump, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);
	return true;
}

bool DealerService::Mt4DealerReconnection(){
	//dealer reconnection.
	m_Dealer_mutex.lock();
	if (RET_OK == m_ExtDealer->IsConnected()){
		SendWarnMail("ERROR", "Dealer CreateMT4LinkInterface reconnect the link");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"m_ExtDealer reconnect the link");
		OutputDebugString("ERR:m_ExtDealer reconnect the link");
		bool flag = CreateMT4LinkInterface(&m_ExtDealer, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);

		if (!flag){
			m_Dealer_mutex.unlock();
			return false;
		}

		int res2 = m_ExtDealer->DealerSwitch(OnDealingFunc, NULL, 0);

		if (RET_OK != res2){
			OutputDebugString("ERR:DealerSwitch: failed!");
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "DealerSwitch: failed! errcode:" << m_ExtManagerPump->ErrorDescription(res2));
			m_Dealer_mutex.unlock();
			return false;
		}
	}

	m_Dealer_mutex.unlock();
	return true;
}

bool DealerService::Mt4PumpReconnection(){
	//pump reconnection 
	m_Pump_mutex.lock();

	if (RET_OK == m_ExtManagerPump->IsConnected()){
		SendWarnMail("ERROR", "Pump CreateMT4LinkInterface reconnect the link");
		OutputDebugString("ERR:Begin:m_ExtManagerPump CreateMT4LinkInterface reconnect the link");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_ExtManagerPump reconnect the link");
		bool flag = CreateMT4LinkInterface(&m_ExtManagerPump, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);

		if (!flag){
			m_Pump_mutex.unlock();
			return false;
		}

		int res1 = m_ExtManagerPump->PumpingSwitchEx(OnPumpingFunc, NULL, 0);

		OutputDebugString("ERR:End:m_ExtManagerPump  PumpingSwitchEx reconnect the link");
		if (RET_OK != res1){
			OutputDebugString("ERR:PumpSwitch: failed!");
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "PumpSwitch: failed errcode :" << m_ExtManagerPump->ErrorDescription(res1));
			m_Pump_mutex.unlock();
			return false;
		}

	}

	m_Pump_mutex.unlock();
	return true;
}

bool DealerService::CreateMT4LinkInterface(CManagerInterface ** m_interface, const string &server, const int &login, const string &passwd)
{
	//begin connect MT4 server 
	int res = 0;
	int cnt = 0;
	char tmp[256] = {0};

	while (cnt < 3){
		if (RET_OK == (*m_interface)->Ping()){
			OutputDebugString("RET_OK == (*m_interface)->Ping()");
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "RET_OK == (*m_interface)->Ping()");
			return true;
		}

		res = (*m_interface)->Connect(server.c_str());

		if (RET_OK != res){
			sprintf(tmp, "ERR:Connect MT4: connect failed. %s\n", (*m_interface)->ErrorDescription(res));
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Connect MT4: connect failed. errcode:" << (*m_interface)->ErrorDescription(res));
			OutputDebugString(tmp);
			Sleep(1);
		}else{
			break;
		}

		cnt++;
	}
	
	if (cnt >= 3){
		return false;
	}

	cnt = 0;

	while (cnt < 3){
		res = (*m_interface)->Login(login, passwd.c_str());

		if (RET_OK != res){
			memset(tmp, 0, 256);
			sprintf(tmp, "ERR:Login MT4: login failed. %s\n", (*m_interface)->ErrorDescription(res));
			OutputDebugString(tmp);
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Login MT4: login failed. errcode:" << (*m_interface)->ErrorDescription(res));
			Sleep(1);
		}else{
			break;
		}

		cnt++;
	}

	if (cnt >= 3){
		return false;
	}

	return true;
}

//create bridge socket link  suport dns process add by wzp 
//bool DealerService::CreateBridgeLink()
//{
//	string tmp_addr = "tcp://";
//	tmp_addr += m_config["bridge_ip"] + ":" + m_config["bridge_port"];
//
//	if (m_socket.connected()){
//		m_socket.connect(tmp_addr);
//	}else{
//		char tmp[256] = { 0 };
//		sprintf(tmp, "ERR:Connect Bridge: Link failed %s", tmp_addr.c_str());
//		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Connect Bridge: Link failed. addr:" << tmp_addr.c_str());
//		OutputDebugString(tmp);
//		return false;
//	}
//
//	return true;
//}

//call back function add by wzp
void __stdcall OnDealingFunc(int code)
{
	int res = RET_ERROR;
	//---
	switch (code)
	{
	case DEAL_START_DEALING:
		OutputDebugString("DEBUG:DEAL_START_DEALING");
		DealerService::GetInstance()->SendWarnMail("WARN", "DEAL_START_DEALING");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"DEAL_START_DEALING");
		break;
	case DEAL_STOP_DEALING:
		OutputDebugString("DEBUG:DEAL_STOP_DEALING");
		DealerService::GetInstance()->SendWarnMail("ERROR", "DEAL_STOP_DEALING");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "DEAL_STOP_DEALING");
		break;
	case DEAL_REQUEST_NEW:
		DealerService::GetInstance()->ProcessMsgUp();
		break;
	default: break;
	}
	//---
	return;
}

void __stdcall OnPumpingFunc(int code, int type, void *data, void *param)
{
	switch (code)
	{
	case PUMP_START_PUMPING:
		OutputDebugString("PUMP_START_PUMPING");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "PUMP_START_PUMPING");
		DealerService::GetInstance()->SendWarnMail("WARN", "PUMP_START_PUMPING");
		break;
	case PUMP_STOP_PUMPING:
		OutputDebugString("PUMP_STOP_PUMPING");
		DealerService::GetInstance()->SendWarnMail("WARN", "PUMP_STOP_PUMPING");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "PUMP_STOP_PUMPING");
		break;
	case PUMP_UPDATE_BIDASK:
		DealerService::GetInstance()->UpdateAskAndBid();
		break;
	case PUMP_UPDATE_SYMBOLS:
		OutputDebugString("PumpingFunc: PUMP_UPDATE_SYMBOLS");
		DealerService::GetInstance()->UpdateSymbols();
		break;
	case PUMP_UPDATE_GROUPS:
		OutputDebugString("PUMP_UPDATE_GROUPS");
		DealerService::GetInstance()->StaticGroupConfigInfo();
		break;
	case PUMP_UPDATE_USERS:
		OutputDebugString("PUMP_UPDATE_USERS");
		break;
	case PUMP_UPDATE_ONLINE:
		OutputDebugString("PUMP_UPDATE_ONLINE");
		break;
	case PUMP_UPDATE_TRADES:
		DealerService::GetInstance()->PumpProcessTradeOrder(PUMP_UPDATE_TRADES, type, data);
		break;
	case PUMP_UPDATE_ACTIVATION:
		DealerService::GetInstance()->PumpProcessTradeOrder(PUMP_UPDATE_ACTIVATION,type,data);
		break;
	case PUMP_UPDATE_MARGINCALL:
		OutputDebugString("PUMP_UPDATE_MARGINCALL");
		break;
	case PUMP_UPDATE_REQUESTS:
		OutputDebugString("PUMP_UPDATE_REQUESTS");
		break;
	case PUMP_UPDATE_PLUGINS:
		OutputDebugString("PUMP_UPDATE_PLUGINS");
		break;
	case PUMP_UPDATE_NEWS:
		OutputDebugString("PUMP_UPDATE_NEWS");
		break;
	case PUMP_UPDATE_MAIL:
		OutputDebugString("PUMP_UPDATE_MAIL");
		break;
	default: break;
	}
	//---
}

//process trade order for 
void DealerService::PumpProcessTradeOrder(int code ,int type, void *data){
	dealer::RequestInfo info{};
	char tmp[256];
	bool send_flag = true;

	//--- checks code type value.
	if (code == PUMP_UPDATE_TRADES){
		//no data return add by wzp
		if (data == NULL){
			sprintf(tmp, "INFO:PumpProcessManagerOrder PUMP_UPDATE_TRADES: data == NULL code:%d type: %d", code, type);
			OutputDebugString(tmp);
			return;
		}

		TradeRecord *trade = (TradeRecord*)data;
		send_flag = PumpProcessManagerOrder(trade, &info, type);
		//send msg to bridge
		if (send_flag){
			OutputDebugString("BEGIN:PUMP_UPDATE_TRADES");
			
			if (!SendDataToBridge(&info, &m_socket_pump)){
				OutputDebugString("SendDataToBridge failed!");
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"SendDataToBridge failed!");
				DealerService::GetInstance()->SendWarnMail("ERROR", "SendDataToBridge");
			}

			OutputDebugString("END:PUMP_UPDATE_TRADES");
			info.Clear();
		}

	} else if (code == PUMP_UPDATE_ACTIVATION){
		send_flag = PumpProcessOtherOrder(type, &info);
	}
}

bool DealerService::FilterRepeatOrder(const TradeRecord &rec,string &grp){
	OrderValue order{0};

	if (rec.activation != ACTIVATION_PENDING && 
		rec.activation != ACTIVATION_SL && 
		rec.activation != ACTIVATION_TP && 
		rec.activation != ACTIVATION_STOPOUT){
		return false;
	}

	//pending open then stop out question. add by wzp 2018-08-03
	if (!m_Orders.Get(rec.order, order) || order.rec.activation != rec.activation){
		//if this record have in the m_Orders we don't need to get group name.
		if (ACTIVATION_NONE != order.rec.activation){
			grp = order.grp_name;
		} else{
			grp = GetUserGroup(rec.login);
			order.grp_name = grp;
		}
		order.rec = rec;
		//order.rec.activation = rec.activation;
		//order.rec.order= rec.order;
		order.timestrap = GetUtcCaressing();
		order.status = 0;
		m_Orders.Add(rec.order, order);
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Add order.rec.order: " << order.rec.order //add by wzp 2018-10-26 
			<< " order.rec.activation: " << order.rec.activation << " order.timestrap: " << order.timestrap);
		return true;
	} else if (order.status == 1){//return mt4 failed
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "FilterRepeatOrder  if (order.status == " << order.status << ") order:" << order.rec.order); //add by wzp 2018 - 10 - 26
		return false;
	} else {
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "FilterRepeatOrder  if (order.status == " << order.status << ") suc order:" << order.rec.order);//add by wzp 2018-10-26
		return false;
	}
}

bool DealerService::CaculateMargin(const TradeRecord &record){
	double margin = 0;
	SymbolConfigInfo info = {0};
	MarginLevel tmpMargin{ 0 };

	string strGrp = GetUserGroup(record.login);

	if ("" == strGrp){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateMargin: GetUserGroup(record.login) failed ! login:" << record.login << " order:" << record.order);
		return false;
	}

	m_Pump_mutex.lock();
	int res = m_ExtManagerPump->MarginLevelGet(record.login, strGrp.c_str(), &tmpMargin);

	if (RET_OK != res){
		m_Pump_mutex.unlock();
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateMargin: GetMarginInfo failed ! login:" << record.login << " order:" << record.order);
		OutputDebugString("ERR:GetMarginInfo failed !");
		return false;
	}

	m_Pump_mutex.unlock();

	if (tmpMargin.margin_free <= 0.0){
	//	PumpSendDataToMT4(record);
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateMargin: if (tmpMargin.margin_free <= 0.0) login:" << record.login << " order:" << record.order);
		return false;
	}

	if (!m_SymbolConfigInfo.Get(record.symbol, info)){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "if (!m_SymbolConfigInfo.Get(record.symbol, info):" << record.symbol);
		return false;
	}
	//get config info about the symbol
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "info.type:" << info.type);
	//The "UK" need special caculation modify add by wzp 2018-09-07
	if (info.type == CFD || (string::npos != strGrp.find("UK") && (info.type == ENERGY || info.type == METAL))){
		margin = (record.volume * 0.01) * info.contract_size * record.open_price / 100.00 * (100.00 / info.margin_divider) / 100.00;
	} else if (info.type == ENERGY || info.type == METAL){
		margin = (record.volume * 0.01) * info.margin_initial * (100.00 / info.margin_divider) / 100.00;
	} else{
		margin = (record.volume * 0.01) * info.contract_size / 100.00 * (100.00 / info.margin_divider) / 100.00;
	}

	if (record.margin_rate > 0){
		margin = margin * record.margin_rate;
	} else if (!CurrencyConversion(margin, strGrp, info)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateMargin: if (tmpMargin.margin_free <= 0.0) login:" << record.login << " order:" << record.order);
		return false;
	}

	//double profit = (record.close_price - record.open_price) * (record.volume * 0.01) * info.contract_size;
//	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "usd :profit:" << profit);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmpMargin.balance:" << tmpMargin.balance << "tmpMargin.equity:" << tmpMargin.equity << "tmpMargin.margin_free:" << tmpMargin.margin_free);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "MarginLevelGet:(tmpMargin.margin_free - margin):" << (tmpMargin.margin_free - margin) << " margin:" << margin);
	//double profit = 
	if ((tmpMargin.margin_free - margin) <= 0.0){
	//	PumpSendDataToMT4(record);
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateMargin: if ((tmpMargin.margin_free - margin) <= 0.0):" << record.login << " order:" << record.order);
		return false;
	}

	return true;
}
//margin Currency conversion add by wzp 2018-07-30
bool DealerService::CurrencyConversion(double &margin, const string &group, const SymbolConfigInfo &info){
	GroupConfigInfo tmpGroup;
	//begin judge the margin currency and the balance currency.
	if (!m_GroupConfigInfo.Get(group, tmpGroup)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CurrencyConversion：m_GroupConfigInfo.Get(group, tmpGroup) no the info" << group);
		return false;
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_GroupConfigInfo:" << tmpGroup.currency);

	if (!ConvUnit(margin, info.margin_currency, tmpGroup.currency)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (ConvUnit(margin, info.margin_currency, tmpGrp.currency)): Err");
		return false;
	}

	return true;
}

//transfer unit for
bool DealerService::ConvUnit(double &value, const string &from, const string &to){
	//return true;
	if (from == to){
		return true;
	}

	SymbolInfo tmpSi = { 0 };
	SymbolsValue tmpSv = { 0 };
	string tmpBefore = from + to;
	string tmpAfter = to + from;

	if (m_Symbols.Get(tmpBefore, tmpSv)){
		if (!GetSiInfo(tmpBefore, tmpSi)){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "ConvUnit: Err GetSiInfo" << tmpBefore);
			return false;
		}
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmpBefore ConvUnit: info GetSiInfo" << value << ":" << tmpBefore);
		value = value*((tmpSi.ask + tmpSi.bid) / 2.00);
		return true;
	} else if (m_Symbols.Get(tmpAfter, tmpSv)){
		if (!GetSiInfo(tmpAfter, tmpSi)){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "ConvUnit: Err GetSiInfo" << tmpAfter);
			return false;
		}
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmpAfter ConvUnit: info GetSiInfo " << value << ":" << tmpAfter);
		value = value/((tmpSi.ask + tmpSi.bid) / 2.00);
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmpAfter ConvUnit: info GetSiInfo " << value << ":" << tmpAfter);
		return true;
	} else{
		if (ConvUnit(value, from, "USD")){
			return ConvUnit(value, "USD", to);
		} else{
			return false;
		}
	}
}

bool DealerService::StaticSymbolConfigInfo(){
	int cnt = 0;
	string tmp;
	regex rex_fx(REX_FX);
	regex rex_metal(REX_METAL);
	regex rex_energy(REX_ENERGY);
	regex rex_cfd(REX_CFD);

	m_ExtManager_mutex.lock();
	ConSymbol *m_ConSymbol = m_ExtManager->CfgRequestSymbol(&cnt);

	if (m_ConSymbol == NULL){
		SwitchConnection();
		m_ConSymbol = m_ExtManager->CfgRequestSymbol(&cnt);

		if (m_ConSymbol == NULL){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (m_ConSymbol == NULL)");
			m_ExtManager_mutex.unlock();
			return false;//"-1" is failed get data from mt4
		}
	}

	for (int i = 0; i < cnt; i++){
		if (std::regex_match(m_ConSymbol[i].symbol, rex_fx)){
			tmp = FX;
		} else if (std::regex_match(m_ConSymbol[i].symbol, rex_metal)){
			tmp = METAL;
		} else if (std::regex_match(m_ConSymbol[i].symbol, rex_energy)){
			tmp = ENERGY;
		} else if (std::regex_match(m_ConSymbol[i].symbol, rex_cfd)){
			tmp = CFD;
		}

		SymbolConfigInfo tmpSymbolConfig{0};
		tmpSymbolConfig.contract_size = m_ConSymbol[i].contract_size;
		tmpSymbolConfig.margin_divider = m_ConSymbol[i].margin_divider;
		tmpSymbolConfig.margin_initial = m_ConSymbol[i].margin_initial;
		tmpSymbolConfig.margin_currency = m_ConSymbol[i].margin_currency;
		tmpSymbolConfig.type = tmp;
		m_SymbolConfigInfo.Add(m_ConSymbol[i].symbol, tmpSymbolConfig);
	}

	m_ExtManager->MemFree(m_ConSymbol);
	m_ConSymbol = NULL;
	m_ExtManager_mutex.unlock();
	return true;
}

bool DealerService::StaticGroupConfigInfo(){
	int cnt = 0;
	GroupConfigInfo tmpCfg{};
	m_ExtManager_mutex.lock();
	ConGroup *m_ConGroup = m_ExtManager->CfgRequestGroup(&cnt);

	if (m_ConGroup == NULL){
		SwitchConnection();
		m_ConGroup = m_ExtManager->CfgRequestGroup(&cnt);

		if (m_ConGroup == NULL){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (m_ConSymbol == NULL)");
			m_ExtManager_mutex.unlock();
			return false;//"-1" is failed get data from mt4
		}
	}

	for (int i = 0; i < cnt; i++){
		tmpCfg.currency = m_ConGroup[i].currency;
		memcpy(&tmpCfg.secgroups, &m_ConGroup[i].secgroups, sizeof(m_ConGroup[i].secgroups));
		m_GroupConfigInfo.Add(m_ConGroup[i].group, tmpCfg);
		memset(&tmpCfg, 0, sizeof(tmpCfg));
	}

	m_ExtManager->MemFree(m_ConGroup);
	m_ConGroup = NULL;
	m_ExtManager_mutex.unlock();
	return true;
}

bool DealerService::PumpProcessOtherOrder(int type, dealer::RequestInfo *info)
{
	int  activated = 0;
	char tmp[256];
	bool send_flag = true;
	vector<string>::iterator ita;
	string grp = "";
	//get tr.ade info with the Pump mode from MT4. add  by wzp
	m_Pump_mutex.lock();
	m_TradeRecord = m_ExtManagerPump->TradesGet(&activated);
	m_Pump_mutex.unlock();

	if (activated)
	{
		for (int i = 0; i < activated; i++){
			if (m_TradeRecord[i].activation == ACTIVATION_NONE || !JudgeSymbol(m_TradeRecord[i].symbol))
				continue;
			if (!FilterRepeatOrder(m_TradeRecord[i], grp))
				continue;
			if (m_TradeRecord[i].activation == ACTIVATION_PENDING ){
				send_flag = CaculateMargin(m_TradeRecord[i]);

				if (send_flag){
					TransferRecordToProto(m_TradeRecord[i], grp, info, TT_BR_ORDER_ACTIVATE, D_PE);
				} else{//pending order cancel process. add by wzp 2018-08-13
					PumpSendDataToMT4(m_TradeRecord[i], TT_BR_ORDER_DELETE, grp);
				}
				
			} else if (m_TradeRecord[i].activation == ACTIVATION_SL){
				send_flag = true;
			//	DealerLog::GetInstance()->LogInfo("ACTIVATION_SL");
				TransferRecordToProto(m_TradeRecord[i], grp,info, TT_BR_ORDER_CLOSE, D_SL);
			} else if (m_TradeRecord[i].activation == ACTIVATION_TP ){
				send_flag = true;
				TransferRecordToProto(m_TradeRecord[i], grp,info, TT_BR_ORDER_CLOSE, D_TP);
			} else if (m_TradeRecord[i].activation == ACTIVATION_STOPOUT){
				send_flag = true;
				TransferRecordToProto(m_TradeRecord[i], grp,info, TT_BR_ORDER_CLOSE, D_SO);
			} else{
				send_flag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "send_flag = false");
			}

			PrintRecord(m_TradeRecord[i], grp);
			//send msg to bridge
			if (send_flag){
				if (!SendDataToBridge(info, &m_socket_pump)){
					OutputDebugString("ERR:SendDataToBridge failed!");
				}

				info->Clear();
			}
		}

		m_Pump_mutex.lock();
		m_ExtManagerPump->MemFree(m_TradeRecord);
		m_TradeRecord = NULL;
		m_Pump_mutex.unlock();

	} else{
		send_flag = false;
	}

	return send_flag;
}

bool DealerService::PumpProcessManagerOrder(TradeRecord * trade, dealer::RequestInfo *info,const int type){
	//1.Is not manager or manager api order
	if (trade->reason != TR_REASON_DEALER && trade->reason != TR_REASON_API){
		return false;
	}

	//2.manager or manager api.
	if (type != TRANS_ADD && type != TRANS_DELETE){
		return false;
	}

	//3.cmd is 0 or 1
	if (trade->cmd != OP_BUY && trade->cmd != OP_SELL){
		return false;
	}

	//4.sysmbol judge 
	if (!JudgeSymbol(trade->symbol)){
		return false;
	}

	string tmp = GetUserGroup(trade->login);
	//5.group judge ABOOK
	if ("" == tmp || A_BOOK != GetGroupType(tmp)){
		return false;
	}
	//6.judge valid time open time or close time in 10 seconds.
	time_t now = time(NULL) + (time_t)10800;
	time_t cap = abs(now - trade->timestamp);

	if (cap >= 10){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "timestamp cap >= 10 order of manager or manager api don't process,now:" << now << " cap:" << cap);
		return false;
	}
	//7.if the time cap large the 3 seconds.
	if (cap >= 3){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "timestamp cap >= 3 order of manager or manager api beyond 3 seconds");
	}
	//8.judge the sl tp and so close operate don't process the order.
	if ((NULL != strstr(trade->comment, D_SL) ||NULL != strstr(trade->comment, D_TP) ||
		NULL != strstr(trade->comment, "so:")) && (type == TRANS_DELETE)){
		OutputDebugString("INFO:PumpProcessManagerOrder a sl or tp or so order from manager ");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:PumpProcessManagerOrder a sl or tp or so order from manager.type:" <<
			type << ",cmd:" << trade->cmd << ",order:" << trade->order << ",comment:" << trade->comment);
		return false;
	}

	PrintRecord(*trade, tmp);
	//begin--------add by wzp 2018-11-30 the manager make order
	string coment = trade->comment;
	strcpy(trade->comment, "MANAGER");
	m_Orders.Add(trade->order, OrderValue{ GetUtcCaressing(), 0, tmp, *trade});
	strcpy(trade->comment, coment.c_str());
	//end----------add by wzp 2018-11-30 the manager make order
	switch (type)
	{
	case TRANS_ADD:
		OutputDebugString("TRANS_ADD");
		TransferRecordToProto(*trade, tmp, info, TT_BR_ORDER_OPEN, "MANAGER");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:#" << trade->order << " added-------------------------");
		break;
	case TRANS_DELETE:
		OutputDebugString("TRANS_DELETE");
		TransferRecordToProto(*trade, tmp, info, TT_BR_ORDER_CLOSE, "MANAGER");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:#" << trade->order << " closed-------------------------");
		break;
	case TRANS_UPDATE:
		OutputDebugString("TRANS_UPDATE");
		return false;
	}

	return true;
}

string DealerService::GetUserGroup(const int login){
	int total;
	string strTmp;
	UserRecord tmp{ 0 };
	m_Pump_mutex.lock();
	int res = m_ExtManagerPump->UserRecordGet(login, &tmp);

	if (RET_OK != res){
		m_Pump_mutex.unlock();
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "GetUserGroup failed ! login:" << login);
		OutputDebugString("ERR:GetUserGroup failed !");
		return "";
	}

	m_Pump_mutex.unlock();
	OutputDebugString(tmp.group);
	strTmp = tmp.group;
	
	return strTmp;
}

void DealerService::UpdateSymbols()
{
	ConSymbol *cs = NULL;
	int        total = 0, i;
	//---
	m_Pump_mutex.lock();
	cs = m_ExtManagerPump->SymbolsGetAll(&total);

	for (i = 0; i<total; i++){
		m_ExtManagerPump->SymbolAdd(cs[i].symbol);
		m_Symbols.Add(cs[i].symbol, SymbolsValue{ cs[i].type, cs[i].point, cs[i].digits });
	}

	if (cs != NULL) { 
		m_ExtManagerPump->MemFree(cs); cs = NULL; total = 0; 
	}

	m_Pump_mutex.unlock();
	//record all the Symbol config info.add by wzp 2018-07-30
	StaticSymbolConfigInfo();
}
//current not use UpdateAskAndBid add by wzp;
void DealerService::UpdateAskAndBid()
{
	int total = 0;
	SymbolInfo si[32];

	m_Pump_mutex.lock();
	total = m_ExtManagerPump->SymbolInfoUpdated(si, 32);
	m_Pump_mutex.unlock();
}

void DealerService::ProcessMsgUp(){
	m_Dealer_mutex.lock();
	int res = m_ExtDealer->DealerRequestGet(&m_req);
	char tmp[256] = { 0 };

	if (RET_OK != res){
		sprintf(tmp, "ERR:ProcessMsgUp: get data failed. %s\n", m_ExtDealer->ErrorDescription(res));
		OutputDebugString(tmp);
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, tmp); 
		DealerService::GetInstance()->SendWarnMail("ERROR", "m_ExtDealer->DealerRequestGet(&m_req)");
		m_Dealer_mutex.unlock();
		return;
	}

	time_t tim_tmp = GetUtcCaressing();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Begin:GetUtcCaressing:" << tim_tmp);
	PrintRequest(&m_req);

	//add business judge process add by wzp. 2018.04.26
	if (!BusinessJudge(&m_req)){
		memset(&m_req, 0, sizeof(struct RequestInfo));
		m_Dealer_mutex.unlock();
		return;
	}

	m_Dealer_mutex.unlock();
	dealer::RequestInfo data{};
	//info transfer to protocbuf msg
	if (!TransferMsgToProto(&data)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "TransferMsgToProto:TransferMsgToProto failed! order:" << m_req.id);
		OutputDebugString("ERR:TransferMsgToProto:TransferMsgToProto failed!");
		memset(&m_req, 0, sizeof(struct RequestInfo));
		return;
	}

	m_Reqs.Add(m_req.id, ReqValue{ m_req, GetUtcCaressing() });//add by wzp 2018-11-28 record the request info.
	//send data to bridge
	if (!SendDataToBridge(&data, &m_socket_dealer)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"ProcessMsgUp:send data to bridge failed!");
		OutputDebugString("ERR:ProcessMsgUp:send data failed!");
	}

	memset(&m_req, 0, sizeof(struct RequestInfo));
}

bool DealerService::BusinessJudge(RequestInfo *req){
	SymbolInfo si{};
	//add by if recv the symbol dealer don't process the order. add by wzp 2018-06-28
	if (!JudgeSymbol(req->trade.symbol)){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"BusinessJudge:!JudgeSymbol(req->trade.symbol)!");
		m_ExtDealer->DealerReject(req->id);
		DealerService::GetInstance()->SendWarnMail("WARN", "JudgeSymbol dealer don't process the symbol");
		return false;
	}

	if (TT_ORDER_MODIFY == req->trade.type || TT_ORDER_PENDING_OPEN == req->trade.type 
		|| TT_ORDER_DELETE == req->trade.type || TT_ORDER_CLOSE_ALL == req->trade.type 
		|| TT_ORDER_CLOSE_BY == req->trade.type){
		//OutputDebugString("TT_ORDER_MODIFY");

		if (CaculateAskBidAndSpread(*req, false)){
			m_ExtDealer->DealerSend(req, false, false);
		} else{
			DealerService::GetInstance()->SendWarnMail("ERROR", "！CaculateAskBidAndSpread");
			m_ExtDealer->DealerReset(req->id);
		}
		return false;//not need send data to bridge
	} else {
		return true;
	}
}

//bool  DealerService::SendDataToBridge( dealer::RequestInfo *msg)
//{
//	string send_buf;
//	zmq::context_t ctx(1);
//	zmq::socket_t sock(ctx, ZMQ_DEALER);
//	string tmp_addr ="tcp://"+ m_config["bridge_ip"] + ":" + m_config["bridge_port"];
//	try{
//
//		sock.connect(tmp_addr);
//	
//		if (msg == NULL){
//			OutputDebugString("ERR:SendDataToBridge");
//			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"SendDataToBridge failed!");
//			return false;
//		}
//		//compose msg 
//		msg->SerializeToString(&send_buf);
//		zmq::message_t reply(send_buf.length());
//		memcpy((void*)reply.data(), send_buf.c_str(), send_buf.length());
//		//send msg
//		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Begin SendDataToBridge:send data  ! req_id:" << m_req.id << "order:" << m_req.trade.order);
//		if (!sock.send(reply, ZMQ_DONTWAIT)){
//			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "SendDataToBridge:send data failed ! req_id:" << m_req.id << "order:" << m_req.trade.order);
//		}
//		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "End SendDataToBridge:send data  ! req_id:" << m_req.id << "order:" << m_req.trade.order);
//
//		sock.disconnect(tmp_addr);
//		sock.close();
//	}
//	catch (exception &e){
//		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "SendDataToBridge:send data  ! req_id:" << m_req.id << " order: " << m_req.trade.order<<"ex:" << e.what());
//	}
//	return true;
//
//}
//create socket  add by wzp 2018-12-07
zmq::socket_t * DealerService::CreateClientSocket(zmq::context_t & context) {
	std::cout << "I: connecting to server..." << std::endl;
	zmq::socket_t * client = new zmq::socket_t(context, ZMQ_REQ);
	string tmp_addr = "tcp://" + m_config["bridge_ip"] + ":" + m_config["bridge_port"];
	client->connect(tmp_addr);

	//  Configure socket to not wait at close time
	int linger = 0;
	client->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));

	return client;
}

bool  DealerService::SendDataToBridge(dealer::RequestInfo *msg, zmq::socket_t **Client){
	if (Client == NULL){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "if (Client == NULL){");
		return false;
	}

	if ((*Client) == NULL){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Init *Client = CreateClientSocket(m_context)");
		*Client = CreateClientSocket(m_context);
	}
	//zmq::socket_t *Client = CreateClientSocket(m_context);
	int retries_left = REQUEST_RETRIES;
	//encode msg process
	string request;
	std::stringstream sequence;
	sequence << msg->id() << "_" << msg->trade().order();
	string strsequence = sequence.str();
	msg->SerializeToString(&request);
	//send msg to server.
	if (!s_send(**Client, request)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "s_send(*Client, request.str()))");
	}
	//begin recv reply msg from sever.
	bool expect_reply = true;

	while (expect_reply) {
		//  Poll socket for a reply, with timeout
		zmq::pollitem_t Items[] = {{ **Client, 0, ZMQ_POLLIN, 0 }};
		zmq::poll(&Items[0], 1, REQUEST_TIMEOUT);

		//  If we got a reply, process it
		if (Items[0].revents & ZMQ_POLLIN) {
			//  We got a reply from the server, must match sequence
			string reply = s_recv(**Client);

			if (reply == strsequence) {
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "I: server replied OK (" << reply << ")");
			//	std::cout << "I: server replied OK (" << reply << ")" << std::endl;
				//	retries_left = REQUEST_RETRIES;
				expect_reply = false;
			//	delete Client;
				return true;
			} else {
				//std::cout << "E: malformed reply from server: " << reply << std::endl;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "E: malformed reply from server: ");
			}
		} else if (--retries_left == 0) {
			//std::cout << "E: server seems to be offline, abandoning" << std::endl;
			LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "E: server seems to be offline, abandoning");
			expect_reply = false;
			break;
		} else {
		//	std::cout << "W: no response from server, retrying..." << std::endl;
			LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "W: no response from server, retrying...");
			//  Old socket will be confused; close it and open a new one
			delete (*Client);
			(*Client) = CreateClientSocket(m_context);
			//  Send request again, on new socket
			if (!s_send(**Client, request)){
				//std::cout << "s_send(*Client, request.str()))" << std::endl;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "s_send(*Client, request.str()))");
			}
		}
	}

//	delete Client;
	return false;
}

void DealerService::InitThread(){

	CreateThread(NULL, 0, TransferMsgFromBridgeToMT4, NULL, 0, NULL);

	CreateThread(NULL, 0, KeepLiveForMT4, NULL, 0, NULL);
}


DWORD WINAPI TransferMsgFromBridgeToMT4(LPVOID lparamter){
	DealerService::GetInstance()->ProcessMsgDown();
	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "TransferMsgFromBridgeToMT4");
	return 1;
}


DWORD WINAPI KeepLiveForMT4(LPVOID lparamter){
	DealerService::GetInstance()->KeepLiveLinkMT4();
	return 1;
}

void DealerService::KeepLiveLinkMT4(){
	bool flag = false;

	while (true){
		OutputDebugString("INFO:KeepLiveLinkMT4");
		//direct reconnection 
		if (!Mt4DirectReconnection()){
			continue;
		}

		//pump reconnection 
		if (!Mt4PumpReconnection()){
			continue;
		}

		//dealer reconnection.
		if (!Mt4DealerReconnection()){
			continue;
		}
		
		//delete sl tp so temp order
		DeleteOrderRecord();//add by wzp 2018-07-04
		//DealerService::GetInstance()->SendWarnMail("Test","KeepLiveLinkMT4");//test to use 2018-07-25
		CheckDealerReqInfo();
		//check if have some Request don't immediately process delay to 5 seconds.
		Sleep(2000);
	}
}

void DealerService::CheckDealerReqInfo(){
	// this is the normal condition no Req info in the queue.
	if (m_Reqs.Empty()){
		return;
	}

	cout << "m_Reqs.Empty()" <<endl;
	bool Flag = false;
	stack<int> st;
	m_Reqs.m_mutex.lock();
	map<int, ReqValue>::iterator iter = m_Reqs.m_queue.begin();
	//check all the ReqInfo if over the time TIME_REQ_GAP need to process and send ReqInfo to mt4.
	while (iter != m_Reqs.m_queue.end()){
		time_t now = GetUtcCaressing();

		if ((now - (iter->second.timestrap)) >= TIME_REQ_GAP){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "now - iter->second.timestrap >= TIME_REQ_GAP");
			m_Reqs.m_mutex.unlock();
			DealerSend(iter->second.ReqInfo, 2);
			m_Reqs.m_mutex.lock();
			st.push(iter->first);
			Flag = true;
		}
		
		iter++;
	}

	m_Reqs.m_mutex.unlock();
	//if have some Request info delay Need to send warn mail 
	if (Flag){
		Count++;
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Dealer RequestInfo delay or lost msg from Bridge");
		SendWarnMail("ERROR", "Dealer RequestInfo delay or lost msg from Bridge");
	}else{
		Count = 0;
		return;
	}
	//del some Request info from the Req queue.
	while (!st.empty()){
		m_Reqs.Delete(st.top());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "CheckDealerReqInfo ReqID:" << st.top()); //add by wzp 2018 - 10 - 26
		st.pop();
	}
	//if appear three times delay to process Request I need Restart Service.
	if (Count == 3){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "stop dealer service and restart service");
		SendWarnMail("ERROR", "Stop dealer service and restart service");
		Sleep(2000);
		abort();
	}
}

bool DealerService::Mt4DirectReconnection(){
	//direct link 
	m_ExtManager_mutex.lock();

	if (RET_OK != m_ExtManager_bak->Ping()){
		//use ping keep the direct connection live 
		//if the direct connection is disconnected 
		OutputDebugString("ERR:m_ExtManager reconnect the link");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"m_ExtManager reconnect the link");
		//direct reconnection.
		bool flag = CreateMT4LinkInterface(&m_ExtManager_bak, m_config["mt4_addr"], atoi(m_config["mt4_login_name"].c_str()), m_config["mt4_passwd"]);

		if (!flag){
			m_ExtManager_mutex.unlock();
			return false;
		}
	}

	m_ExtManager_mutex.unlock();
	return true;
}


void DealerService::DeleteOrderRecord(){
	//OutputDebugString("Begin:DeleteOrderRecord");
	m_Orders.m_mutex.lock();
	char tmp[256];
	map<int, OrderValue>::iterator iter = m_Orders.m_queue.begin();
	stack<int> st;
	
	while (iter != m_Orders.m_queue.end()){
		sprintf(tmp, "second.timestrap:%lld", iter->second.timestrap);
		OutputDebugString(tmp);
		memset(tmp,0,sizeof(tmp));
		time_t nTmp = GetUtcCaressing() - iter->second.timestrap;
		sprintf(tmp, "diff:%lld", nTmp);
		OutputDebugString(tmp);
		if ((nTmp >= TIME_GAP && 2 == iter->second.status) || nTmp >= TIME_GAP_FAIL){ //add by wzp 2018-10-16 delete the modify
			//need to delete this order.
			st.push(iter->first);
		} else if (0 == iter->second.status && nTmp >= TIME_GAP){
			//if recv msg from bridge delay need auto to process
			if (!strcmp(iter->second.rec.comment,"MANAGER")){
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "DeleteOrderRecord Pump find manager order can not recv from Bridge Order:" << iter->second.rec.order);
				SendWarnMail("ERROR", "DeleteOrderRecord Pump find manager order can not recv from Bridge Order");
				st.push(iter->first);
				iter++;
				continue;
			}

			int type = 0;

			if (iter->second.rec.activation == ACTIVATION_PENDING){
				type = TT_BR_ORDER_ACTIVATE;
			} else{
				type = TT_BR_ORDER_CLOSE;
			}

			m_Orders.m_mutex.unlock();
			if (!PumpSendDataToMT4(iter->second.rec, type, iter->second.grp_name)){
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "DeleteOrderRecord Pump auto process order to MT4 info orderID:"<<iter->second.rec.order);
				SendWarnMail("ERROR", "DeleteOrderRecord Pump auto process order to MT4 info");
			}
			m_Orders.m_mutex.lock();
		}

		iter++;
	}

	if (!st.empty()){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "DeleteOrderRecord m_Orders count:" << m_Orders.m_queue.size());
	}

	while (!st.empty()){
		m_Orders.m_queue.erase(st.top());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "DeleteOrderRecord OrderNum:" << st.top()); //add by wzp 2018 - 10 - 26
		st.pop();
	}

	m_Orders.m_mutex.unlock();
}

//bool DealerService::ProcessMsgDown()
//{
//	dealer::resp_msg msg{};
//	zmq::message_t request;
//
//	zmq::context_t ctx(1);
//	zmq::socket_t sock(ctx, ZMQ_DEALER);
//	//string tmp_addr = "tcp://*:8101";
//	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tcp://*:" + m_config["local_port"]);
//	sock.bind("tcp://*:" + m_config["local_port"]);
//
//	while (true){
//		sock.recv(&request);
//		std::string data(static_cast<char*>(request.data()), request.size());
//		//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "sock.recv(&request)!");
//		if (!msg.ParseFromString(data)){
//			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Bridge: ParseFromString failed !");
//			OutputDebugString("ERR:Bridge: ParseFromString failed !");
//		} else {
//			if (!SendDataToMT4(msg)){	//send info to MT4
//				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Bridge: send data to MT4 failed !");
//				OutputDebugString("ERR:Bridge: send data to MT4 failed !");
//			}
//		}
//
//		msg.Clear();
//		request.rebuild();
//	}
//
//	return true;
//}

bool DealerService::ProcessMsgDown(){
	zmq::socket_t Server = zmq::socket_t(m_context, ZMQ_REP);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tcp://*:" + m_config["local_port"]);
	try{
		Server.bind("tcp://*:" + m_config["local_port"]);
	}
	catch (std::exception e){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Server.bind error! " << e.what());
	}
	
	dealer::resp_msg msg{};

	while (1) {
		std::string request = s_recv(Server);
		std::cout << "I: normal request (" << request << ")" << std::endl;

		if (!msg.ParseFromString(request)){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Bridge: ParseFromString failed !");
			OutputDebugString("ERR:Bridge: ParseFromString failed !");
		} else {
			stringstream sequence;
			sequence << msg.info().id()<< "_"<< msg.info().trade().order();
			//reply to client bridge
			s_send(Server, sequence.str());

			if (!SendDataToMT4(msg)){	//send info to MT4
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Bridge: send data to MT4 failed !");
				OutputDebugString("ERR:Bridge: send data to MT4 failed !");
			}
		}
	}

	return true;
}

void DealerService::Close(void)
{
	//if (m_socket.connected())
	//{
	//	string tmp_addr = "tcp:://";
	//	tmp_addr += m_config["bridge_ip"] + ":" + m_config["bridge_port"];
	//	m_socket.disconnect(tmp_addr);
	//	m_socket.close();
	//}
}

bool DealerService::SendDataToMT4(const dealer::resp_msg &ret)
{
	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:SendDataToMT4-----------------:" << ret.info().id());
//	OutputDebugString("INFO:BEGIN:SendDataToMT4-------------");
	//manager order dont need reply to mt4.//here need note some 
	if (string::npos != ret.info().trade().comment().find(MANAGER)){
		m_Orders.Delete(ret.info().trade().order());//delete order from the m_Orders
		OutputDebugString("SendDataToMT4 find manager response");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:SendDataToMT4 this is manager open order:" << ret.info().trade().order() << " bridge price:" << ret.info().trade().price());
		return true;
	}

	if (string::npos != ret.info().trade().comment().find(D_PE) ||
		string::npos != ret.info().trade().comment().find(D_SL) ||
		string::npos != ret.info().trade().comment().find(D_TP) ||
		string::npos != ret.info().trade().comment().find("so:")){
		
		if (!PumpSendDataToMT4(ret)){
			DealerService::GetInstance()->SendWarnMail("ERROR", "PumpSendDataToMT4");
			return false;
		}
	} else{
		if (!DealerSendDataToMT4(ret)){
			DealerService::GetInstance()->SendWarnMail("ERROR", "DealerSendDataToMT4");
			return false;
		}
	}

	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:SendDataToMT4-----------------:" << ret.info().id());
	//OutputDebugString("IINFO:END:SendDataToMT4-------------");
	return true;
}


bool DealerService::DealerSendDataToMT4(const dealer::resp_msg &ret){
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:DealerSendDataToMT4-----------------:" << ret.info().id());
//	OutputDebugString("INFO:BEGIN:DealerSendDataToMT4-------------");
	int res = 0;
	bool flag = true;
	//dealer process message
	
	switch (ret.ret_type())
	{
	case SEND:
		if (!TransferProtoToMsg(&ret)){
			flag = false;
		} else{
			flag = DealerSend(m_req_recv, ret.finish_status());
		}
		break;
	case REJECT:
		DealerReject(ret.info().id());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReject() req_id:" << ret.info().id() << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
		break;
	case RESET:
		DealerReset(ret.info().id());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReset() req_id:" << ret.info().id() << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
		break;
	default:
		break;
	}

	m_Reqs.Delete(ret.info().id());//add by wzp 2018-11-28 when recv the msg from bridge need del the 
	memset(&m_req_recv, 0, sizeof(struct RequestInfo));
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:DealerSendDataToMT4---------------------------:" << ret.info().id());
//	OutputDebugString("END:SendDataToMT4-------------");
	return flag;
}

bool DealerService::DealerSend(RequestInfo &info, const int finish_status){
	//the mormal condition the m_Reqs have a bakup info about Req info from mt4 when this msg back from bridge will del this req info.
	if (!m_Reqs.IsExit(info.id)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "this Reqest to confirm is too late to process RequestInfo.id: " << info.id << ", order:%d" << m_req_recv.trade.order);
		return false;
	}

	if (0.0 >= info.trade.price){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (0.0 == m_req_recv.trade.price) req_id:" << info.id << ", order:%d" << m_req_recv.trade.order);
		m_Dealer_mutex.lock();
		m_ExtDealer->DealerReject(info.id);
		m_Dealer_mutex.unlock();
		DealerService::GetInstance()->SendWarnMail("ERROR", "0.0 == m_req_recv.trade.price");
		return false;
	}

	if (!CaculateAskBidAndSpread(info)){
		memset(&info, 0, sizeof(struct RequestInfo));
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "BackFillAskAndBid failed ! req_id:" << info.id << ", order:%d" << info.trade.order);
		OutputDebugString("ERR:BackFillAskAndBid failed !");
		m_ExtDealer->DealerReject(info.id);
		MakeNewOrderForClient(info.trade, info.login);//add new order for client.2018-06-25
		DealerService::GetInstance()->SendWarnMail("ERROR", "CaculateAskBidAndSpread DealerSend have a error");
		memset(&info, 0, sizeof(struct RequestInfo));
		return false;
	}

	PrintResponse(finish_status, &info);
	m_Dealer_mutex.lock();
	int res = m_ExtDealer->DealerSend(&info, true, false);//test

	if (res != RET_OK){
		OutputDebugString("ERR: m_ExtDealer->DealerSend(&m_req_recv, true, false)");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "code:" << m_ExtDealer->ErrorDescription(res) << " req_id:" << info.id << ", order: " << info.trade.order << " m_ExtDealer->DealerSend(&m_req_recv, true, false)");
		m_ExtDealer->DealerReject(info.id);
		MakeNewOrderForClient(info.trade, info.login);//add new order for client.2018-06-25
		DealerService::GetInstance()->SendWarnMail("ERROR", "DealerSend have a error");
		memset(&info, 0, sizeof(struct RequestInfo));
	}

	m_Dealer_mutex.unlock();
	time_t tmp = GetUtcCaressing();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "End:GetUtcCaressing::" << tmp);
	return true;
}

bool DealerService::DealerReject(const int id){
	m_Dealer_mutex.lock();
	m_ExtDealer->DealerReject(id);
	m_Dealer_mutex.unlock();
//	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReject() req_id:" << id << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
	return true;
}

bool DealerService::DealerReset(const int id){
	m_Dealer_mutex.lock();
	m_ExtDealer->DealerReset(id);
	m_Dealer_mutex.unlock();
	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReset() req_id:" << ret.info().id() << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
	return true;
}

bool DealerService::PumpSendDataToMT4(const dealer::resp_msg &ret){
	//manager process trade type add by wzp.
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:PumpSendDataToMT4-----------------");
	OutputDebugString("INFO:BEGIN:PumpSendDataToMT4-------------");
	TradeTransInfo info{};

	if (!TransferProtoToTrade(&info, ret)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "No End TransferProtoToTrade PumpSendDataToMT4-----------------order_id:" << ret.info().trade().order());
		OutputDebugString("ERR:No End TransferProtoToTrade PumpSendDataToMT4-------------");
	//	ModifyOrderRecord(info.order, 1);
		return false;
	}

	if (ret.ret_type() == REJECT){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "ERR:this order:" << ret.info().trade().order() << " be rejected by bridge reject reason:" << ret.comment().c_str());
		//ModifyOrderRecord(info.order, 2);//add by wzp 2018-10-11 If Bridge Reject need modify the order status for "1" .
		return false;
	}

	PrintTradeInfo(&info);
	m_ExtManager_mutex.lock();
	int res = m_ExtManager->TradeTransaction(&info);
	
	if (RET_OK != res){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_ExtManager->TradeTransaction(&info) failed! order:" << info.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
		OutputDebugString("ERR:m_ExtManager->TradeTransaction(&info) failed!");
		SwitchConnection();
		res = m_ExtManager->TradeTransaction(&info);
		
		if (RET_OK != res){
			ModifyOrderRecord(info.order,1);
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "SwitchConnection failed! order:" << info.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
			OutputDebugString("ERR:SwitchConnection failed!");
			m_ExtManager_mutex.unlock();
			return false;
		}
	}

	ModifyOrderRecord(info.order, 2);
	m_ExtManager_mutex.unlock();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:PumpSendDataToMT4-----------------");
	OutputDebugString("INFO:END:PumpSendDataToMT4-------------");

	return true;
}

bool DealerService::PumpSendDataToMT4(const TradeRecord &record,UCHAR type,const string &group){
	TradeTransInfo info{};
	cout << "begin auto process to mt4" << endl;
	info.type = type;
	info.order = record.order;
	info.cmd = record.cmd;
	strcpy(info.symbol, record.symbol);
	info.volume = record.volume;
	info.price = record.open_price;
	info.sl = record.sl;
	info.tp = record.tp;
	info.expiration = record.expiration;//add by wzp 2018-05-20.
	strcpy(info.comment, record.comment);
	//if del a order i don't caculate spread.
	if (type != TT_BR_ORDER_DELETE){
		if (!CaculateSpreadForPump(&info, group)){
			return false;
		}
	}

	cout << "end auto process to mt4" << endl;
	PrintTradeInfo(&info);
	m_ExtManager_mutex.lock();

	int res = m_ExtManager->TradeTransaction(&info);

	if (RET_OK != res){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_ExtManager->TradeTransaction(&info) failed! order:" << info.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
		OutputDebugString("ERR:m_ExtManager->TradeTransaction(&info) failed!");
		SwitchConnection();
		res = m_ExtManager->TradeTransaction(&info);

		if (RET_OK != res){
			ModifyOrderRecord(info.order, 1);
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "SwitchConnection failed! order:" << info.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
			OutputDebugString("ERR:SwitchConnection failed!");
			m_ExtManager_mutex.unlock();
			return false;
		}
	}

	ModifyOrderRecord(info.order, 2);
	m_ExtManager_mutex.unlock();

	return true;
}

void DealerService::ModifyOrderRecord(const int &OrderNum, const int &status){
	OrderValue order{0};

	if (m_Orders.Get(OrderNum, order)){
		order.status = status;
		order.timestrap = GetUtcCaressing();
		m_Orders.Add(OrderNum, order);
	}
}

bool DealerService::MakeNewOrderForClient(const TradeTransInfo &rec, const int &login){
	string tmp = GetUserGroup(login);
	//5.group judge ABOOK
	if ("" == tmp || A_BOOK != GetGroupType(tmp)){
		return false;
	}

	if (rec.type == TT_BR_ORDER_ACTIVATE || rec.type == TT_BR_ORDER_OPEN || rec.type == TT_ORDER_MK_OPEN){
		TradeTransInfo info{};
		info.cmd = rec.cmd;
		strcpy(info.symbol, rec.symbol);
		info.orderby = login;
		info.volume = rec.volume;
		info.price = rec.price;

		info.type = TT_BR_ORDER_OPEN;
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Begin-----MakeNewOrderForClient-----");
		PrintTradeInfo(&info);
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "End-------MakeNewOrderForClient-----");
		m_ExtManager_mutex.lock();
		SwitchConnection();//add by wzp 2018-11-16
		int res = m_ExtManager->TradeTransaction(&info);
		m_ExtManager_mutex.unlock();

		if (RET_OK != res){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "MakeNewOrderForClient failed! order:" << rec.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
		}
	}

	return true;
}

void DealerService::SwitchConnection(){
	//int res = m_ExtManager->Ping();
	//--begin--judge the connection is exist ---add by wzp---2018-06-21
	if (RET_OK == m_ExtManager->Ping()){
		return;
	}
	//--end--judge the connection is exist ---add by wzp---2018-06-21
	if (m_ExtManager == m_pool[0]){
		m_ExtManager = m_pool[1];
		m_ExtManager_bak = m_pool[0];
	} else{
		m_ExtManager = m_pool[0];
		m_ExtManager_bak = m_pool[1];
	}
}

bool DealerService::TransferProtoToTrade(TradeTransInfo *info, const dealer::resp_msg &msg){
	//if there are the splited the order only receive the last one to process.

	info->type = msg.info().trade().type();
	info->order = msg.info().trade().order();
	info->cmd = msg.info().trade().cmd();
	strcpy(info->symbol, msg.info().trade().symbol().c_str());
	info->volume = msg.info().trade().volume();
	info->price = msg.info().trade().price();
	info->sl = msg.info().trade().sl();
	info->tp = msg.info().trade().tp();
	info->expiration = msg.info().trade().expiration();//add by wzp 2018-05-20.
	strcpy(info->comment, msg.info().trade().comment().c_str());

	//if (msg.finish_status() != 2){
	//	OutputDebugString("WARN:TransferProtoToTrade msg.finish_status != 2");
	//	LOG4CPLUS_WARN(DealerLog::GetInstance()->m_Logger, "TransferProtoToTrade msg.finish_status != 2 order:" << msg.info().trade().order());
	//	return false;
	//}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "order = " << info->order<<" CaculateSpreadForPump before:" << info->price);

	if (!CaculateSpreadForPump(info,msg.info().group())){
		return false;
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "order = " << info->order << "CaculateSpreadForPump after:" << info->price);

	return FilterSplitOrder(&msg);
}

bool DealerService::CaculateSpreadForPump(TradeTransInfo *info, const string &group){
	//SymbolInfo  si = { "" };
	GroupConfigInfo grp{};
	SymbolsValue sv{};
	//get symbol info about the ask and bid. add by wzp 2018-08-01
	if (!GetSymbolInfo(info->symbol, sv)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (!GetAskBidInfo(msg.info().trade().symbol(), sv)) CaculateSpreadForPump: order_id:" << info->order);
		return false;
	}
	//get the group info about the spread info. add by wzp 2018-08-01
	if (!m_GroupConfigInfo.Get(group, grp)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateSpreadForPump: order_id:" << info->order);
		OutputDebugString("ERR:CaculateSpreadForPump");
		return false;
	}

	double spread_diff = grp.secgroups[sv.type].spread_diff;
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "order_id: " << info->order << " grp.secgroups[sv.type].spread_diff: " << spread_diff);
	if (spread_diff != 0){
		//TT_BR_ORDER_ACTIVATE
		int cmd = info->cmd;
		unsigned int type = info->type;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "order_id: " << info->order << " cmd: " << cmd << " type: " << type);
		if ((cmd == OP_BUY && type == TT_BR_ORDER_OPEN ) ||
			((cmd == OP_BUY_LIMIT || cmd == OP_BUY_STOP) && type == TT_BR_ORDER_ACTIVATE) ||
			(cmd == OP_SELL && type == TT_BR_ORDER_CLOSE)){
			info->price = NormalizeDouble(info->price + sv.point * spread_diff / 2.0, sv.digits);
		} else if ((cmd == OP_SELL && type == TT_BR_ORDER_OPEN) ||
			((cmd == OP_SELL_LIMIT || cmd == OP_SELL_STOP) && type == TT_BR_ORDER_ACTIVATE) ||
			(cmd == OP_BUY && type == TT_BR_ORDER_CLOSE)){
			info->price = NormalizeDouble(info->price - sv.point * spread_diff / 2.0, sv.digits);
		}
	}

	return true;
}

bool DealerService::GetSiInfo(const string &symbol, SymbolInfo &si){
	m_Pump_mutex.lock();
	SymbolsValue sv{ 0 };
	//get symbol info with the m_ExtManagerPump interface.
	if ((m_ExtManagerPump->SymbolInfoGet(symbol.c_str(), &si)) != RET_OK){
		m_ExtManagerPump->SymbolAdd(symbol.c_str());
		OutputDebugString("ERR:m_ExtManagerPump->SymbolInfoGet(m_req.trade.symbol, &si) failed");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_ExtManagerPump->SymbolInfoGet(m_req.trade.symbol, &si) failed::" << symbol.c_str());
		m_Pump_mutex.unlock();
		return false;
	}

	if (si.bid == 0.0 || si.ask == 0.0){
		m_ExtManagerPump->SymbolAdd(symbol.c_str());
		OutputDebugString("ERR:SymbolInfoGet failed si.bid == 0.0 || si.ask == 0.0 ");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "SymbolInfoGet failed si.bid == 0.0 || si.ask == 0.0:");
		m_Pump_mutex.unlock();
		return false;
	}

	m_Pump_mutex.unlock();

	return true;
}

bool DealerService::GetSymbolInfo(const string &symbol, SymbolsValue &sv){

	if (!m_Symbols.Get(symbol, sv)){
		SymbolInfo si{};
		LOG4CPLUS_WARN(DealerLog::GetInstance()->m_Logger, "GetAskBidInfo:if (!m_Symbols.Get(symbol, sv)){");
		if (!GetSiInfo(symbol, si)){
			if (0 == si.type || 0.0 == si.point || 0 == si.digits){
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "GetAskBidInfo tmp.type:  if (!GetSiInfo(symbol, si)) error");
				return false;//only when type point and digits is 0 ,I will retrun false;add by wzp.
			}
		}
		
		m_Symbols.Add(symbol, SymbolsValue{ si.type, si.point, si.digits });
	} else{
	//	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmp.type:" << sv.type << ",tmp.point:" << sv.point << ",tmp.digits:" << sv.digits);
	}

	return true;
}

//
bool DealerService::TransferProtoToMsg(const dealer::resp_msg *msg){
	m_req_recv.id = msg->info().id();
	m_req_recv.status = msg->info().status();
	m_req_recv.time = msg->info().time();
	m_req_recv.manager = msg->info().manager();
	m_req_recv.login = msg->info().login();
	strcpy(m_req_recv.group, msg->info().group().c_str());
	m_req_recv.balance = msg->info().balance();
	m_req_recv.credit = msg->info().credit();
	m_req_recv.trade.type = msg->info().trade().type();
	m_req_recv.trade.flags = msg->info().trade().flags();
	m_req_recv.trade.cmd = msg->info().trade().cmd();
	m_req_recv.trade.order = msg->info().trade().order();
	m_req_recv.trade.orderby = msg->info().trade().orderby();
	strcpy(m_req_recv.trade.symbol, msg->info().trade().symbol().c_str());
	m_req_recv.trade.volume = msg->info().trade().volume();
	m_req_recv.trade.price = msg->info().trade().price();
	m_req_recv.trade.sl = msg->info().trade().sl();
	m_req_recv.trade.tp = msg->info().trade().tp();
	m_req_recv.trade.ie_deviation = msg->info().trade().ie_deviation();
	strcpy(m_req_recv.trade.comment, msg->info().trade().comment().c_str());
	m_req_recv.trade.expiration = msg->info().trade().expiration();

	m_req_recv.gw_volume = msg->info().gw_volume();
	m_req_recv.gw_order = msg->info().gw_order();
	m_req_recv.gw_price = msg->info().gw_price();

	m_req_recv.prices[0] = msg->info().price(0);
	m_req_recv.prices[1] = msg->info().price(1);

	return FilterSplitOrder(msg);
}

//begin process the split order. deal the first reply data.
bool DealerService::FilterSplitOrder(const dealer::resp_msg *msg){

	char tmp[256];
	int value;
	sprintf(tmp, "%d_%d_%d", m_req_recv.id, m_req_recv.login, m_req_recv.trade.order);
	OutputDebugString(tmp);
	//split order process
	if (msg->finish_status() != 2){

		if (!m_SplitOrders.Get(tmp, value)){
			m_SplitOrders.Add(tmp, msg->finish_status());
			return true;
		} else{
			LOG4CPLUS_WARN(DealerLog::GetInstance()->m_Logger, "TransferProtoToMsg  req_id:" << msg->info().id() << " order_id:" << msg->info().trade().order() << " msg.finish_status:" << msg->finish_status());
			return false;
		}
	} else{
		if (m_SplitOrders.Get(tmp, value)){
			m_SplitOrders.Delete(tmp);
			return false;
		}
	}

	return true;
}

bool DealerService::CaculateAskBidAndSpread(RequestInfo &req, bool flag)
{
	SymbolsValue tmpSV = { 0 };
	SymbolInfo  si = { "" };
	GroupConfigInfo tmpGrp;
	//judge disconnect process for Pump. add by wzp
	if (!Mt4PumpReconnection()){
		OutputDebugString("ERR:m_ExtManagerPump BackFillAskAndBid reconnect the link");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"m_ExtManagerPump BackFillAskAndBid reconnect the link");
		return false;
	}

	if (flag && !GetSymbolInfo(req.trade.symbol, tmpSV)){
		OutputDebugString("ERR:GetSpreadAndAskBidInfo failed:");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "GetSpreadAndAskBidInfo: I can not get the symbol info" << req.group);
		return false;
	}

	//modify order and other don't submit order to bridge.
	if (!flag || req.prices[0] == 0.0 || req.prices[1] == 0.0){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "if (flag=" << flag << " || req.prices[0] = " << req.prices[0] << " || req.prices[1] =" << req.prices[1] << ")");
		if (!GetSiInfo(req.trade.symbol, si)){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (!GetSiInfo(req.trade.symbol, si))");
			return false;
		} else{
			req.prices[0] = si.bid; req.prices[1] = si.ask;
			tmpSV.point = si.point; tmpSV.type = si.type; tmpSV.digits = si.digits;
		}

		//sprintf(tmp, "req.prices[0]:%lf,req.prices[1]:%lf", req.prices[0], req.prices[1]);
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "spread caculate begin:req.id=" << req.id <<"req.trade.order="<< req.trade.order<<" req.prices[0]:" << req.prices[0] << ",req.prices[1]:" << req.prices[1] << ",req.trade.price:" << req.trade.price);
	if (!m_GroupConfigInfo.Get(req.group, tmpGrp)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_GroupConfigInfo: get the group info error: " << req.group);
		return false;
	}
	
	double spread_diff = tmpGrp.secgroups[tmpSV.type].spread_diff;
	//set the prices value to use the current bridge price.
	if ((req.trade.cmd == OP_BUY && req.trade.type == TT_ORDER_MK_OPEN) || 
		(req.trade.cmd == OP_SELL && req.trade.type == TT_ORDER_MK_CLOSE)){
		req.prices[1] = req.trade.price;
		if (spread_diff!= 0.0){
			req.prices[1] = NormalizeDouble(req.prices[1] + tmpSV.point * spread_diff / 2.0, tmpSV.digits);
			req.trade.price = req.prices[1];
			req.prices[0] = NormalizeDouble(req.prices[0] - tmpSV.point * spread_diff / 2.0, tmpSV.digits);
		}
	} else if ((req.trade.cmd == OP_SELL && req.trade.type == TT_ORDER_MK_OPEN) || 
		(req.trade.cmd == OP_BUY && req.trade.type == TT_ORDER_MK_CLOSE)){
		req.prices[0] = req.trade.price;
		if (spread_diff != 0.0){
			req.prices[0] = NormalizeDouble(req.prices[0] - tmpSV.point * spread_diff / 2.0, tmpSV.digits);
			req.trade.price = req.prices[0];
			req.prices[1] = NormalizeDouble(req.prices[1] + tmpSV.point * spread_diff / 2.0, tmpSV.digits);
		}
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "spread caculate end:req.id=" << req.id << " req.trade.order=" << req.trade.order << " req.prices[0]:" << req.prices[0] << ",req.prices[1]:" << req.prices[1] << ",req.trade.price:" << req.trade.price);
	return true;
}

double __fastcall NormalizeDouble(const double val, int digits)
{
	if (digits<0) digits = 0;
	if (digits>8) digits = 8;
	//---
	const double p = ExtDecimalArray[digits];
	return((val >= 0.0) ? (double(__int64(val*p + 0.5000001)) / p) : (double(__int64(val*p - 0.5000001)) / p));
}

bool DealerService::TransferRecordToProto(const TradeRecord &record,const string &grp, dealer::RequestInfo *msg, unsigned int type, string comment){
	msg->set_login(record.login);
	//---Begin---get ABOOK or BBOOK type --------add by wzp 2018-06-21
	//string strTmp = GetUserGroup(record.login);
	msg->set_group(grp);
	msg->set_group_type(GetGroupType(grp));
	//---End---get ABOOK or BBOOK type ---------add by wzp 2018-06-21
	msg->mutable_trade()->set_type(type);
	msg->mutable_trade()->set_order(record.order);
	msg->mutable_trade()->set_cmd(record.cmd);
	msg->mutable_trade()->set_symbol(record.symbol);
	msg->mutable_trade()->set_volume(record.volume);
	//---begin---open price need send to bridge----add by wzp----2018-05-08
	if (record.activation == ACTIVATION_PENDING || (TT_BR_ORDER_OPEN == type && MANAGER == comment)){
		msg->set_time(record.open_time);
		msg->mutable_trade()->set_price(record.open_price);
	}
	//---end---open price need send to bridge----add by wzp----2018-05-08
	if (record.activation == ACTIVATION_PENDING || 
		record.activation == ACTIVATION_STOPOUT || 
		record.activation == ACTIVATION_SL || MANAGER == comment){
		msg->mutable_trade()->set_sl(record.sl);
	}

	if (record.activation == ACTIVATION_PENDING || 
		record.activation == ACTIVATION_STOPOUT || 
		record.activation == ACTIVATION_TP || MANAGER == comment){
		msg->mutable_trade()->set_tp(record.tp);
	}

	if (record.activation == ACTIVATION_SL || 
		record.activation == ACTIVATION_TP || 
		record.activation == ACTIVATION_STOPOUT ||
		(TT_BR_ORDER_CLOSE == type && MANAGER == comment)){
		msg->set_time(record.timestamp);
		msg->mutable_trade()->set_price(record.close_price);
	}

	string tmp = record.comment;
	//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "record.comment:" << record.comment);
	//i need apend the comment info to comment tail.
	//begin------stop out comment added------ by wzp 2018-06-27
	if (record.activation == ACTIVATION_STOPOUT){
		tmp = GetMarginInfo(record, grp);
	} else {
		tmp = comment;//modify by wzp
	}
	//end------stop out comment added-------- by wzp 2018-06-27
	msg->mutable_trade()->set_comment(tmp.c_str());
	return true;
}

string DealerService::GetMarginInfo(const TradeRecord &record,const string &group){
	MarginLevel tmp{ 0 };
	char format_str[128];
	m_Pump_mutex.lock();
	int res = m_ExtManagerPump->MarginLevelGet(record.login, group.c_str(), &tmp);

	if (RET_OK != res){
		m_Pump_mutex.unlock();
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "GetMarginInfo failed ! login:" << record.login);
		OutputDebugString("ERR:GetMarginInfo failed !");
		return "";
	}
	m_Pump_mutex.unlock();
	sprintf(format_str, "so:%0.1f%%/%0.1f/%0.1f", double((tmp.equity / tmp.margin)*100), tmp.equity, tmp.margin);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "GetMarginInfo:" << format_str);
	return format_str;
}

//the RequestInfo info transfer to protocbuff info add by wzp
bool  DealerService::TransferMsgToProto(dealer::RequestInfo *msg)
{
	msg->set_id(m_req.id);
	msg->set_status(m_req.status);
	msg->set_time(m_req.time);
	msg->set_manager(m_req.manager);
	msg->set_login(m_req.login);
	msg->set_group(m_req.group);
	msg->set_balance(m_req.balance);
	msg->set_credit(m_req.credit);
	msg->set_group_type(GetGroupType(m_req.group));//add by wzp 2018-05-09
	msg->add_price(m_req.prices[0]);//modify by wzp 2018-05-09
	msg->add_price(m_req.prices[1]);//modify by wzp 2018-05-09

	msg->mutable_trade()->set_type(m_req.trade.type);
	msg->mutable_trade()->set_flags(m_req.trade.flags);
	msg->mutable_trade()->set_cmd(m_req.trade.cmd);
	msg->mutable_trade()->set_order(m_req.trade.order);
	msg->mutable_trade()->set_orderby(m_req.trade.orderby);
	msg->mutable_trade()->set_symbol(m_req.trade.symbol);
	msg->mutable_trade()->set_volume(m_req.trade.volume);
	msg->mutable_trade()->set_price(m_req.trade.price);
	msg->mutable_trade()->set_sl(m_req.trade.sl);
	msg->mutable_trade()->set_tp(m_req.trade.tp);
	msg->mutable_trade()->set_ie_deviation(m_req.trade.ie_deviation);
	msg->mutable_trade()->set_comment(m_req.trade.comment);
	msg->mutable_trade()->set_expiration(m_req.trade.expiration);
	msg->mutable_trade()->set_crc(m_req.trade.crc);

	msg->set_gw_volume(m_req.gw_volume);
	msg->set_gw_order(m_req.gw_order);
	msg->set_gw_price(m_req.gw_price);

	return true;
}

int DealerService::GetGroupType(const string group){
	//int flag = 0;
	if (JudgeRegex(m_ARegexArray, group)){
		//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Abook" << group);
		return A_BOOK;
	}

	if (JudgeRegex(m_BRegexArray, group)){
		//LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "Bbook" << group);
		return B_BOOK;
	}


	//vector<string>::iterator ita;
	//string tmp;

	//for (ita = m_abook.begin(); ita != m_abook.end(); ita++){
	//	tmp = *ita;
	//	OutputDebugString(tmp.c_str());
	//	if (string::npos != group.find(*ita)){
	//		OutputDebugString("abook");
	//		return A_BOOK;
	//	}
	//}

	//for (ita = m_bbook.begin(); ita != m_bbook.end(); ita++){
	//	tmp = *ita;
	//	OutputDebugString(tmp.c_str());
	//	if (string::npos != group.find(*ita)){
	//		OutputDebugString("bbook");
	//		return B_BOOK;
	//	}
	//}

	return B_BOOK;
}

//bool DealerService::JudgeSymbol(const string symbol){
//	vector<string>::iterator ita;
//	string tmp;
//	bool InFlag = true,result = false;
//	int index_start = 0, index_end = 0, index_ex = 0;
//	int cnt = 0;
//	for (ita = m_symbols.begin(); ita != m_symbols.end(); ita++){
//		tmp = *ita;
//		OutputDebugString(tmp.c_str());
//		index_ex = tmp.find_first_of("!");
//
//		if (0 == index_ex){
//			InFlag = false;
//		}
//
//		index_start = tmp.find_first_of("*");
//		index_end = tmp.find_last_of("*");
////		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmp.find_first_of():" << index_start << " tmp.find_last_of():" << index_end);
//		if (index_start < index_end){
//			tmp = "[\\s\\S]" + tmp.substr(index_start, index_end - index_start) + "[\\s\\S]*";
//		} else if (index_start == index_end){
//			if (-1 == index_start){//no * symbol
//
//			} else if (index_start > 1){
//				tmp = tmp.substr(0, index_end) + "[\\s\\S]*";
//			} else {
//				tmp = "[\\s\\S]" + tmp.substr(index_start, tmp.length());
//			}
//		}
//
//		//begin modify by wzp 2018-09-11
//		if (!InFlag){
//			if (0 == tmp.find_first_of("!") && tmp.length() >= 2){
//				tmp = tmp.substr(index_ex+1, tmp.length());
//			}
//		}
//
//		//SymbolRegex sr{ InFlag, regex(tmp), tmp };
//		//m_RegexArray.Add(cnt, sr);
//		//end modify by wzp 2018-09-11
//
//	//	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "JudgeSymbol(req->trade.symbol):" << tmp);
//		regex r(tmp);
//
//		if (std::regex_match(symbol, r)){
//			OutputDebugString("I need to process the order");
//			if (!InFlag){
//				result = false;
//			} else{
//				result = true;
//			}
//
//			return result;
//		} else {
//			InFlag = true;
//			result = false;
//		}
//	}
//
//	return result;
//}
bool DealerService::JudgeRegex( MutexMap<int, SymbolGroupRegex> &regexArray, const string &info){
	bool result = false;
	map<int, SymbolGroupRegex>::iterator iter = regexArray.m_queue.begin();

	while (iter != regexArray.m_queue.end()){
		regex r(iter->second.rule);

		if (std::regex_match(info, r)){
			OutputDebugString("I need to process the order");
			if (!iter->second.bReverse){
				result = false;
			} else{
				result = true;
			}

			return result;
		} else {
			result = false;
		}

		iter++;
	}

	return result;
}

bool DealerService::JudgeSymbol(const string symbol){
	return  JudgeRegex(m_SRegexArray, symbol);
	//bool result = false;
	//map<int, SymbolGroupRegex>::iterator iter = m_SRegexArray.m_queue.begin();

	//while (iter != m_SRegexArray.m_queue.end()){
	//	regex r(iter->second.rule);

	//	if (std::regex_match(symbol, r)){
	//		OutputDebugString("I need to process the order");
	//		if (!iter->second.bReverse){
	//			result = false;
	//		} else{
	//			result = true;
	//		}

	//		return result;
	//	} else {
	//		result = false;
	//	}

	//	iter++;
	//}

	//return result;
}

bool DealerService::InitRegex(vector<string> &argVector, MutexMap<int, SymbolGroupRegex> &regexArray){
	vector<string>::iterator ita;
	string tmp;
	bool InFlag = true, result = false;
	int index_start = 0, index_end = 0, index_ex = 0;
	int cnt = 0;

	for (ita = argVector.begin(); ita != argVector.end(); ita++){
		tmp = *ita;
		OutputDebugString(tmp.c_str());
		index_ex = tmp.find_first_of("!");

		if (0 == index_ex){
			InFlag = false;
		}

		index_start = tmp.find_first_of("*");
		index_end = tmp.find_last_of("*");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmp.find_first_of():" << index_start << " tmp.find_last_of():" << index_end);
		if (index_start < index_end){
			tmp = "[\\s\\S]" + tmp.substr(index_start, index_end - index_start) + "[\\s\\S]*";
		} else if (index_start == index_end){
			if (-1 == index_start){//no * symbol

			} else if (index_start > 1){
				tmp = tmp.substr(0, index_end) + "[\\s\\S]*";
			} else {
				tmp = "[\\s\\S]" + tmp.substr(index_start, tmp.length());
			}
		}

		//begin modify by wzp 2018-09-11
		if (!InFlag){
			if (0 == tmp.find_first_of("!") && tmp.length() >= 2){
				tmp = tmp.substr(index_ex + 1, tmp.length());
			}
		}

		regexArray.Add(cnt, SymbolGroupRegex{ InFlag, tmp });
		InFlag = true;
		cnt++;
	}

	return true;
}

static int gettimeofday(struct timeval* tv)
{
	union {
		long long ns100;
		FILETIME ft;
	} now;
	GetSystemTimeAsFileTime(&now.ft);
	tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
	tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL);

	return (0);
}

//获取1970年至今UTC的微秒数
static time_t GetUtcCaressing()
{
	timeval tv;
	gettimeofday(&tv);
	return ((time_t)tv.tv_sec*(time_t)1000000 + tv.tv_usec);
}