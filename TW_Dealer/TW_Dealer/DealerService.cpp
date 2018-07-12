#include "DealerService.h"
#include <direct.h>
#include <time.h>
#include <regex>
#include <sys/timeb.h>


using namespace std;

DealerService *DealerService::m_DealerService = NULL;
string DealerService::m_path = "";
map<string, string> DealerService::m_config;
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
	m_ExtDealer(NULL), m_ExtManagerPump(NULL), m_ExtManager(NULL),m_ExtManager_bak(NULL), m_TradeRecord(NULL),
	m_context(1), m_socket(m_context, ZMQ_DEALER)
{
	m_abook = split(abook, ";");
	m_bbook = split(bbook, ";");
	m_symbols = split(symbols, ";");
	memset(&m_req, 0, sizeof(struct RequestInfo)); 
	memset(&m_req_recv, 0, sizeof(struct RequestInfo));
	memset(m_pool, 0, sizeof(m_pool));
}

DealerService::~DealerService(){
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
bool DealerService::CreateBridgeLink()
{
	string tmp_addr = "tcp://";
	char tmp[256] = { 0 };
	tmp_addr += m_config["bridge_ip"] + ":" + m_config["bridge_port"];

	if (m_socket.connected()){
		m_socket.connect(tmp_addr);
	}else{
		sprintf(tmp, "ERR:Connect Bridge: Link failed %s", tmp_addr.c_str());
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Connect Bridge: Link failed. addr:" << tmp_addr.c_str());
		OutputDebugString(tmp);
		return false;
	}

	return true;
}

//call back function add by wzp
void __stdcall OnDealingFunc(int code)
{
	int res = RET_ERROR;
	//---
	switch (code)
	{
	case DEAL_START_DEALING:
		OutputDebugString("DEBUG:DEAL_START_DEALING");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"DEAL_START_DEALING");
		break;
	case DEAL_STOP_DEALING:
		OutputDebugString("DEBUG:DEAL_STOP_DEALING");
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
		break;
	case PUMP_STOP_PUMPING:
		OutputDebugString("PUMP_STOP_PUMPING");
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
			
			if (!SendDataToBridge(&info)){
				OutputDebugString("SendDataToBridge failed!");
				LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"SendDataToBridge failed!");
			}

			OutputDebugString("END:PUMP_UPDATE_TRADES");
			info.Clear();
		}

	} else if (code == PUMP_UPDATE_ACTIVATION){
		send_flag = PumpProcessOtherOrder(type, &info);
	}
}

bool DealerService::FilterRepeatOrder(const TradeRecord &rec){
	OrderValue order{0};

	if (rec.activation != ACTIVATION_PENDING && 
		rec.activation != ACTIVATION_SL && 
		rec.activation != ACTIVATION_TP && 
		rec.activation != ACTIVATION_STOPOUT){
		return false;
	}

	if (!m_Orders.Get(rec.order, order)){
		order.activation = rec.activation;
		order.OrderNum = rec.order;
		order.timestrap = GetUtcCaressing();
		order.status = 0;
		m_Orders.Add(rec.order, order);
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "order.OrderNum: " << order.OrderNum 
			<< "order.activation: " << order.activation << "order.timestrap: " << order.timestrap);
		return true;
	} else if (order.status == 1){//return mt4 failed
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "FilterRepeatOrder  if (order.status == 1) order:" << order.OrderNum);
		return true;
	} else {
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "FilterRepeatOrder  suc order:" << order.OrderNum);
		return false;
	}
}

bool DealerService::PumpProcessOtherOrder(int type, dealer::RequestInfo *info)
{
	int  activated = 0;
	char tmp[256];
	bool send_flag = true;
	vector<string>::iterator ita;
	//get tr.ade info with the Pump mode from MT4. add  by wzp
	m_Pump_mutex.lock();
	m_TradeRecord = m_ExtManagerPump->TradesGet(&activated);
	m_Pump_mutex.unlock();

	if (activated)
	{
		for (int i = 0; i < activated; i++){
			if (m_TradeRecord[i].activation == ACTIVATION_NONE || !JudgeSymbol(m_TradeRecord[i].symbol))
				continue;
			if (!FilterRepeatOrder(m_TradeRecord[i]))
				continue;
			if (m_TradeRecord[i].activation == ACTIVATION_PENDING){
				send_flag = true;
				TransferRecordToProto(m_TradeRecord[i], info, TT_BR_ORDER_ACTIVATE, "D_PE");
			} else if (m_TradeRecord[i].activation == ACTIVATION_SL){
				send_flag = true;
			//	DealerLog::GetInstance()->LogInfo("ACTIVATION_SL");
				TransferRecordToProto(m_TradeRecord[i], info, TT_BR_ORDER_CLOSE, "D_SL");
			} else if (m_TradeRecord[i].activation == ACTIVATION_TP ){
				send_flag = true;
				TransferRecordToProto(m_TradeRecord[i], info, TT_BR_ORDER_CLOSE, "D_TP");
			} else if (m_TradeRecord[i].activation == ACTIVATION_STOPOUT){
				send_flag = true;
				TransferRecordToProto(m_TradeRecord[i], info, TT_BR_ORDER_CLOSE, "D_SO");
			} else{
				send_flag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "send_flag = false");
			}

			//send msg to bridge
			if (send_flag){
				if (!SendDataToBridge(info)){
					OutputDebugString("ERR:SendDataToBridge failed!");
				}

				info->Clear();
			}
		}

		m_ExtManager_mutex.lock();
		m_ExtManager->MemFree(m_TradeRecord);
		m_TradeRecord = NULL;
		m_ExtManager_mutex.unlock();

	} else{
		send_flag = false;
	}

	return send_flag;
}

bool DealerService::PumpProcessManagerOrder(const TradeRecord * trade, dealer::RequestInfo *info,const int type){
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
	if ((NULL != strstr(trade->comment, "D_SL") ||NULL != strstr(trade->comment, "D_TP") ||
		NULL != strstr(trade->comment, "so:")) && (type == TRANS_DELETE)){
		OutputDebugString("INFO:PumpProcessManagerOrder a sl or tp or so order from manager ");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:PumpProcessManagerOrder a sl or tp or so order from manager.type:" <<
			type << ",cmd:" << trade->cmd << ",order:" << trade->order << ",comment:" << trade->comment);
		return false;
	}

	PrintRecord(*trade);

	switch (type)
	{
	case TRANS_ADD:
		OutputDebugString("TRANS_ADD");
		TransferRecordToProto(*trade, info, TT_BR_ORDER_OPEN, "MANAGER");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:#" << trade->order << " added-------------------------");
		break;
	case TRANS_DELETE:
		OutputDebugString("TRANS_DELETE");
		TransferRecordToProto(*trade, info, TT_BR_ORDER_CLOSE, "MANAGER");
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

	//send data to bridge
	if (!SendDataToBridge(&data)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"ProcessMsgUp:send data failed!");
		OutputDebugString("ERR:ProcessMsgUp:send data failed!");
	}

	//clear m_req;
	memset(&m_req, 0, sizeof(struct RequestInfo));
}

bool DealerService::BusinessJudge(RequestInfo *req){
	SymbolInfo si{};
	//add by if recv the symbol dealer don't process the order. add by wzp 2018-06-28
	if (!JudgeSymbol(req->trade.symbol)){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger,"BusinessJudge:!JudgeSymbol(req->trade.symbol)!");
		m_ExtDealer->DealerReject(req->id);
		return false;
	}

	if (TT_ORDER_MODIFY == req->trade.type || TT_ORDER_PENDING_OPEN == req->trade.type 
		|| TT_ORDER_DELETE == req->trade.type || TT_ORDER_CLOSE_ALL == req->trade.type 
		|| TT_ORDER_CLOSE_BY == req->trade.type){
		//OutputDebugString("TT_ORDER_MODIFY");

		if (BackFillAskAndBid(*req,false)){
			m_ExtDealer->DealerSend(req, false, false);
		} else{
			m_ExtDealer->DealerReset(req->id);
		}
		
		return false;//not need send data to bridge
	} else {
		return true;
	}
}

bool  DealerService::SendDataToBridge( dealer::RequestInfo *msg)
{
	string send_buf;
	if (msg == NULL){
		OutputDebugString("ERR:SendDataToBridge");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"SendDataToBridge failed!");
		return false;
	}


	msg->SerializeToString(&send_buf);
	//if the connection disconnect i need to reset link. 
	if (!m_socket.connected()){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"the socket disconnect no zmq context req_id:" <<  m_req.id << "order:" << m_req.trade.order);
		return false;
	}

	zmq::message_t reply(send_buf.length());
	memcpy((void*)reply.data(), send_buf.c_str(), send_buf.length());
	bool res = m_socket.send(reply);

	if (!res){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "SendDataToBridge:send data failed ! req_id:" << m_req.id << "order:" << m_req.trade.order);
		return false; 
	}

	return true;
}

void DealerService::InitThread(){

	CreateThread(NULL, 0, TransferMsgFromBridgeToMT4, NULL, 0, NULL);

	CreateThread(NULL, 0, KeepLiveForMT4, NULL, 0, NULL);
}


DWORD WINAPI TransferMsgFromBridgeToMT4(LPVOID lparamter){
	DealerService::GetInstance()->ProcessMsgDown();
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
		Sleep(2000);
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
	OutputDebugString("Begin:DeleteOrderRecord");
	m_Orders.m_mutex.lock();
	map<int, OrderValue>::iterator iter;
	char tmp[256];
	iter = m_Orders.m_queue.begin();
	stack<int> st;

	while (iter != m_Orders.m_queue.end()){
		sprintf(tmp, "second.timestrap:%lld", iter->second.timestrap);
		OutputDebugString(tmp);
		memset(tmp,0,sizeof(tmp));
		time_t nTmp = GetUtcCaressing() - iter->second.timestrap;
		sprintf(tmp, "diff:%lld", nTmp);
		OutputDebugString(tmp);
		if (nTmp >= 2000000 && 2 == iter->second.status){
			st.push(iter->first);
			//m_Orders.m_queue.erase(iter->first);
			LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "DeleteOrderRecord!");
			OutputDebugString("INFO:GetUtcCaressing() - iter->second.timestrap>= 2000000000");
			//iter = m_Orders.m_queue.begin();
		} 

		iter++;
	}

	while (!st.empty()){
		m_Orders.m_queue.erase(st.top());
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "st DeleteOrderRecord!");
		st.pop();
	}

	m_Orders.m_mutex.unlock();
	OutputDebugString("End:DeleteOrderRecord");
}

bool DealerService::ProcessMsgDown()
{
	dealer::resp_msg msg{};
	zmq::message_t request;
	zmq::pollitem_t items[] = {{m_socket,0,ZMQ_POLLIN,0}};

	while (true){
		zmq::poll(items, 1, 0);

		if (items[0].revents & ZMQ_POLLIN){

			bool res = m_socket.recv(&request);

			if (!res){
				OutputDebugString("ERR:Bridge: recv data failed !");
				continue;
			}

			std::string data(static_cast<char*>(request.data()), request.size());

			if (!msg.ParseFromString(data)){
				OutputDebugString("ERR:Bridge: ParseFromString failed !");
			}else{
				if (!SendDataToMT4(msg)){	//send info to MT4
					OutputDebugString("ERR:Bridge: send data to MT4 failed !");
				}
			}

			msg.Clear();
			request.rebuild();
		}
	}

	return true;
}

void DealerService::Close(void)
{
	if (m_socket.connected())
	{
		string tmp_addr = "tcp:://";
		tmp_addr += m_config["bridge_ip"] + ":" + m_config["bridge_port"];
		m_socket.disconnect(tmp_addr);
		m_socket.close();
	}
}

bool DealerService::SendDataToMT4(const dealer::resp_msg &ret)
{
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:SendDataToMT4-----------------:" << ret.info().id());
	OutputDebugString("INFO:BEGIN:SendDataToMT4-------------");
	//manager order dont need reply to mt4.//here need note some 
	if (string::npos != ret.info().trade().comment().find("MANAGER")){
		OutputDebugString("SendDataToMT4 find manager response");
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:PumpSendDataToMT4 order:" << ret.info().id() << " bridge price:" << ret.info().trade().price());
		return true;
	}

	if (string::npos != ret.info().trade().comment().find("D_PE") ||
		string::npos != ret.info().trade().comment().find("D_SL") ||
		string::npos != ret.info().trade().comment().find("D_TP") ||
		string::npos != ret.info().trade().comment().find("so:")){
		
		if (!PumpSendDataToMT4(ret)){
			return false;
		}
	} else{
		if (!DealerSendDataToMT4(ret)){
			return false;
		}
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:SendDataToMT4-----------------:" << ret.info().id());
	OutputDebugString("IINFO:END:SendDataToMT4-------------");
	return true;
}


bool DealerService::DealerSendDataToMT4(const dealer::resp_msg &ret){
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:DealerSendDataToMT4-----------------:" << ret.info().id());
	OutputDebugString("INFO:BEGIN:DealerSendDataToMT4-------------");
	int res = 0;
	bool flag = true;
	//dealer process message
	
	switch (ret.ret_type())
	{
	case SEND:
		DealerSend(ret);
		break;
	case REJECT:
		DealerReject(ret);
		break;
	case RESET:
		DealerReset(ret);
		break;
	default:
		break;
	}

	memset(&m_req_recv, 0, sizeof(struct RequestInfo));
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "END:SendDataToMT4---------------------------:" << ret.info().id());
	OutputDebugString("END:SendDataToMT4-------------");
	return true;
}

bool DealerService::DealerSend(const dealer::resp_msg &ret){
	bool flag = TransferProtoToMsg(&ret);

	if (0.0 == m_req_recv.trade.price){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (0.0 == m_req_recv.trade.price) req_id:" << m_req_recv.id << ", order:%d" << m_req_recv.trade.order);
		m_Dealer_mutex.lock();
		m_ExtDealer->DealerReject(m_req_recv.id);
		m_Dealer_mutex.unlock();
		return false;
	}

	if (!BackFillAskAndBid(m_req_recv)){
		memset(&m_req_recv, 0, sizeof(struct RequestInfo));
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "BackFillAskAndBid failed ! req_id:" << m_req_recv.id << ", order:%d" << m_req_recv.trade.order);
		OutputDebugString("ERR:BackFillAskAndBid failed !");
		MakeNewOrderForClient(m_req_recv.trade, m_req_recv.login);//add new order for client.2018-06-25
		m_ExtDealer->DealerReject(m_req_recv.id);
		memset(&m_req_recv, 0, sizeof(struct RequestInfo));
		return false;
	}

	PrintResponse(ret, &m_req_recv);

	if (flag){
		m_Dealer_mutex.lock();
		int res = m_ExtDealer->DealerSend(&m_req_recv, true, false);//test

		if (res != RET_OK){
			OutputDebugString("ERR: m_ExtDealer->DealerSend(&m_req_recv, true, false)");
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "code:" << m_ExtDealer->ErrorDescription(res) << " req_id:" << m_req_recv.id << ", order: " << m_req_recv.trade.order << " m_ExtDealer->DealerSend(&m_req_recv, true, false)");
			MakeNewOrderForClient(m_req_recv.trade, m_req_recv.login);//add new order for client.2018-06-25
			m_ExtDealer->DealerReject(m_req_recv.id);
			memset(&m_req_recv, 0, sizeof(struct RequestInfo));
		}

		m_Dealer_mutex.unlock();
	}

	time_t tmp = GetUtcCaressing();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "End:GetUtcCaressing::" << tmp);
	return true;
}

bool DealerService::DealerReject(const dealer::resp_msg &ret){
	m_Dealer_mutex.lock();
	m_ExtDealer->DealerReject(ret.info().id());
	m_Dealer_mutex.unlock();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReject() req_id:" << ret.info().id() << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
	return true;
}

bool DealerService::DealerReset(const dealer::resp_msg &ret){
	m_Dealer_mutex.lock();
	m_ExtDealer->DealerReset(ret.info().id());
	m_Dealer_mutex.unlock();
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_ExtDealer->DealerReset() req_id:" << ret.info().id() << " order:" << ret.info().trade().order() << " bridge_reason:" << ret.comment().c_str());
	return true;
}

bool DealerService::PumpSendDataToMT4(const dealer::resp_msg &ret){
	//manager process trade type add by wzp.
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "BEGIN:PumpSendDataToMT4-----------------");
	OutputDebugString("INFO:BEGIN:PumpSendDataToMT4-------------");
	TradeTransInfo info{};

	if (ret.ret_type() == REJECT){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "ERR:this order:" << ret.info().trade().order() << " be rejected by bridge reject reason:" << ret.comment().c_str());
		return false;
	}

	if (!TransferProtoToTrade(&info, ret)){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "No End TransferProtoToTrade PumpSendDataToMT4-----------------order_id:" << ret.info().trade().order());
		OutputDebugString("ERR:No End TransferProtoToTrade PumpSendDataToMT4-------------");
		return true;
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
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "End-------MakeNewOrderForClient-------");
		m_ExtManager_mutex.lock();
		int res = m_ExtManager->TradeTransaction(&info);
		m_ExtManager_mutex.unlock();

		if (RET_OK != res){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "MakeNewOrderForClient failed! order:" << rec.order << " ErrCode:" << m_ExtManager->ErrorDescription(res));
		}
	}

	return true;
}

void DealerService::SwitchConnection(){
	int res = m_ExtManager->Ping();
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
	if (msg.finish_status() != 2){
		OutputDebugString("WARN:TransferProtoToTrade msg.finish_status != 2");
		LOG4CPLUS_WARN(DealerLog::GetInstance()->m_Logger, "TransferProtoToTrade msg.finish_status != 2 order:" << msg.info().trade().order());
		//return false;
	}

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
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "CaculateSpreadForPump before:" << info->price);

	if (!CaculateSpreadForPump(info, msg)){
		return false;
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "CaculateSpreadForPump after:" << info->price);
	return true;
}

bool DealerService::CaculateSpreadForPump(TradeTransInfo *info, const dealer::resp_msg &msg){
	SymbolInfo  si = { "" };
	double spread_diff = 0;

	if (GetSpreadAndAskBidInfo(info->symbol, msg.info().group().c_str(), si, spread_diff)){
		if (spread_diff != 0){
			if ((msg.info().trade().cmd() == OP_BUY && msg.info().trade().type() == TT_BR_ORDER_OPEN) ||
				(msg.info().trade().cmd() == OP_SELL && msg.info().trade().type() == TT_BR_ORDER_CLOSE)){
				info->price = NormalizeDouble(info->price + si.point * spread_diff / 2.0, si.digits);
			} else if ((msg.info().trade().cmd() == OP_SELL && msg.info().trade().type() == TT_BR_ORDER_OPEN) ||
				(msg.info().trade().cmd() == OP_BUY && msg.info().trade().type() == TT_BR_ORDER_CLOSE)){
				info->price = NormalizeDouble(info->price - si.point * spread_diff / 2.0, si.digits);
			}
		}
	} else{
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "CaculateSpreadForPump: order_id:" << info->order);
		OutputDebugString("ERR:CaculateSpreadForPump");
		return false;
	}

	return true;
}

bool DealerService::GetSiInfo(const string &symbol, SymbolInfo &si){
	m_Pump_mutex.lock();
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
		m_Symbols.Add(symbol, SymbolsValue{ si.type ,si.point, si.digits });
		m_Pump_mutex.unlock();
		return false;
	}

	m_Symbols.Add(symbol, SymbolsValue{ si.type, si.point, si.digits });
	m_Pump_mutex.unlock();

	return true;
}

bool DealerService::GetSpreadAndAskBidInfo(const string &symbol, const string &group, SymbolInfo &si, double &spread_diff){
	ConGroup     grp = { "" };
	SymbolsValue tmp = {0};

	if (!m_Symbols.Get(symbol, tmp)){
		if (!GetSiInfo(symbol, si)){
			return false;
		}
	} else{
		si.type = tmp.type;
		si.point = tmp.point;
		si.digits = tmp.digits;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "tmp.type:" << tmp.type << ",tmp.point:" << tmp.point << ",tmp.digits:" << tmp.digits);
	}

	m_Pump_mutex.lock();
	int res = m_ExtManagerPump->GroupRecordGet(group.c_str(), &grp);
	//if spread diff is zero return.
	if (res != RET_OK){
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "m_ExtManagerPump->GroupRecordGet failed RET_OK :%s"<<symbol.c_str());
		m_Pump_mutex.unlock();
		return false;
	}

	if (0 == grp.secgroups[si.type].spread_diff){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "0 == grp.secgroups[si.type].spread_diff = 0");
		m_Pump_mutex.unlock();
		return true;
	}

	m_Pump_mutex.unlock();
	spread_diff = grp.secgroups[si.type].spread_diff;
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
	//split order process
	if (msg->finish_status() != 2){
		OutputDebugString("WARN:TransferProtoToMsg msg.finish_status != 2");
		LOG4CPLUS_WARN(DealerLog::GetInstance()->m_Logger, "TransferProtoToMsg  req_id:" << msg->info().id() << " order_id:" << msg->info().trade().order() << " msg.finish_status:" << msg->finish_status());
	}

	return true;
}
bool DealerService::BackFillAskAndBid(RequestInfo &req,bool flag)
{
	SymbolInfo  si = { "" };
	double spread_diff = 0.0;

	//judge disconnect process for Pump. add by wzp
	if (!Mt4PumpReconnection())
	{
		OutputDebugString("ERR:m_ExtManagerPump BackFillAskAndBid reconnect the link");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger,"m_ExtManagerPump BackFillAskAndBid reconnect the link");
		return false;
	}

	char tmp[256];
	sprintf(tmp, "req.prices[0]:%lf,req.prices[1]:%lf", req.prices[0], req.prices[1]);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, tmp);
	OutputDebugString(tmp);

	if (!GetSpreadAndAskBidInfo(req.trade.symbol, req.group, si, spread_diff)){
		OutputDebugString("ERR:GetSpreadAndAskBidInfo failed:");
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "GetSpreadAndAskBidInfo:" << req.group);
		return false;
	}

	//modify order and other don't submit order to bridge.
	if (!flag || req.prices[0] == 0.0 || req.prices[1] == 0.0){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "if (!flag || req.prices[0] == 0.0 || req.prices[1] == 0.0)");
		if (!GetSiInfo(req.trade.symbol, si)){
			LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "if (!GetSiInfo(req.trade.symbol, si))");
			return false;
		} else{
			req.prices[0] = si.bid; req.prices[1] = si.ask;
		}

		sprintf(tmp, "req.prices[0]:%lf,req.prices[1]:%lf", req.prices[0], req.prices[1]);
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, tmp);
	}

	//set the prices value to use the current bridge price.
	if ((req.trade.cmd == OP_BUY && req.trade.type == TT_ORDER_MK_OPEN) || 
		(req.trade.cmd == OP_SELL && req.trade.type == TT_ORDER_MK_CLOSE)){
		req.prices[1] = req.trade.price;
		if (spread_diff!= 0.0){
			req.prices[1] = NormalizeDouble(req.prices[1] + si.point * spread_diff / 2.0, si.digits);
			req.trade.price = req.prices[1];
			req.prices[0] = NormalizeDouble(req.prices[0] - si.point * spread_diff / 2.0, si.digits);
		}
	} else if ((req.trade.cmd == OP_SELL && req.trade.type == TT_ORDER_MK_OPEN) || 
		(req.trade.cmd == OP_BUY && req.trade.type == TT_ORDER_MK_CLOSE)){
		req.prices[0] = req.trade.price;
		if (spread_diff != 0.0){
			req.prices[0] = NormalizeDouble(req.prices[0] - si.point * spread_diff / 2.0, si.digits);
			req.trade.price = req.prices[0];
			req.prices[1] = NormalizeDouble(req.prices[1] + si.point * spread_diff / 2.0, si.digits);
		}
	}

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

bool DealerService::TransferRecordToProto(const TradeRecord &record, dealer::RequestInfo *msg, unsigned int type, string comment){
	msg->set_login(record.login);
	//---Begin---get ABOOK or BBOOK type --------add by wzp 2018-06-21
	string strTmp = GetUserGroup(record.login);
	msg->set_group(strTmp);
	msg->set_group_type(GetGroupType(strTmp));
	//---End---get ABOOK or BBOOK type ---------add by wzp 2018-06-21
	msg->mutable_trade()->set_type(type);
	msg->mutable_trade()->set_order(record.order);
	msg->mutable_trade()->set_cmd(record.cmd);
	msg->mutable_trade()->set_symbol(record.symbol);
	msg->mutable_trade()->set_volume(record.volume);
	//---begin---open price need send to bridge----add by wzp----2018-05-08
	if (record.activation == ACTIVATION_PENDING || (TT_BR_ORDER_OPEN == type && "MANAGER" == comment)){
		msg->set_time(record.open_time);
		msg->mutable_trade()->set_price(record.open_price);
	}
	//---end---open price need send to bridge----add by wzp----2018-05-08
	if (record.activation == ACTIVATION_PENDING || 
		record.activation == ACTIVATION_STOPOUT || 
		record.activation == ACTIVATION_SL || "MANAGER" == comment){
		msg->mutable_trade()->set_sl(record.sl);
	}

	if (record.activation == ACTIVATION_PENDING || 
		record.activation == ACTIVATION_STOPOUT || 
		record.activation == ACTIVATION_TP || "MANAGER" == comment){
		msg->mutable_trade()->set_tp(record.tp);
	}

	if (record.activation == ACTIVATION_SL || 
		record.activation == ACTIVATION_TP || 
		record.activation == ACTIVATION_STOPOUT ||
		(TT_BR_ORDER_CLOSE == type && "MANAGER" == comment)){
		msg->set_time(record.timestamp);
		msg->mutable_trade()->set_price(record.close_price);
	}

	string tmp = record.comment;
	//i need apend the comment info to comment tail.
	//begin------stop out comment added------ by wzp 2018-06-27
	if (record.activation == ACTIVATION_STOPOUT){
		tmp = GetMarginInfo(record, strTmp);
	} else{	
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
	vector<string>::iterator ita;
	string tmp;

	for (ita = m_abook.begin(); ita != m_abook.end(); ita++){
		tmp = *ita;
		OutputDebugString(tmp.c_str());
		if (string::npos != group.find(*ita)){
			OutputDebugString("abook");
			return A_BOOK;
		}
	}

	for (ita = m_bbook.begin(); ita != m_bbook.end(); ita++){
		tmp = *ita;
		OutputDebugString(tmp.c_str());
		if (string::npos != group.find(*ita)){
			OutputDebugString("bbook");
			return B_BOOK;
		}
	}

	return B_BOOK;
}

bool DealerService::JudgeSymbol(const string symbol){
	vector<string>::iterator ita;
	string tmp;
	bool InFlag = true,result = false;
	int index_start = 0, index_end = 0, index_ex = 0;

	for (ita = m_symbols.begin(); ita != m_symbols.end(); ita++){
		tmp = *ita;
		OutputDebugString(tmp.c_str());
		index_ex = tmp.find_first_of("!");
		if (0 == index_ex){
			InFlag = false;
		}

		index_start = tmp.find_first_of("*");
		index_end = tmp.find_last_of("*");

		if (index_start < index_end){
			tmp = "[\\s\\S]" + tmp.substr(index_start, index_end - index_start) + "[\\s\\S]*";
		} else if (index_start == index_end){
			if (-1 == index_start){//no * symbol

			} else if (index_start > 1){
				tmp = tmp.substr(0, index_end) + "[\\s\\S]*";
			} else if (InFlag) {
				tmp = "[\\s\\S]" + tmp.substr(index_start, tmp.length());
			}
		}

		if (!InFlag){
			tmp = "!" + tmp;
		}

		regex r(tmp);

		if (std::regex_match(symbol, r)){
			OutputDebugString("I need to process the order");
			result = true;
			return result;
		}
	}

	return result;
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

static time_t GetUtcCaressing()
{
	timeval tv;
	gettimeofday(&tv);
	return ((time_t)tv.tv_sec*(time_t)1000000 + tv.tv_usec);
}