#include "log4_cplus.h"


DealerLog *DealerLog::m_DealerLog = NULL;
log4cplus::Initializer initializer;

void DealerLog::initialize(const string &strPath){
	helpers::LogLog::getLogLog()->setInternalDebugging(true);
	log4cplus::tstring pattern = LOG4CPLUS_TEXT("[%D{%Y-%m-%d %H:%M:%S,%Q}] [%t] %-5p - %m%n ");
	m_appender = SharedFileAppenderPtr(new DailyRollingFileAppender(strPath, DAILY, true, 7));
	m_appender->setName(LOG4CPLUS_TEXT("First"));
	m_appender->setLayout(std::unique_ptr<Layout>(new PatternLayout(pattern)));
	m_appender->getloc();
	m_Logger = Logger::getInstance(LOG4CPLUS_TEXT("Log"));
	m_Logger.addAppender(SharedAppenderPtr(m_appender.get()));
	//LOG4CPLUS_ERROR(m_Logger, "asdfasdfasdfasdfasdf");
}


DealerLog *DealerLog::GetInstance(const string &path) {
	//log4cplus::Initializer initializer;
	
	if (NULL == m_DealerLog){
		cout << "DealerLog:" << path << endl;
		m_DealerLog = new DealerLog(path);
	}

	return m_DealerLog;
}

DealerLog::DealerLog(const string &path){
	_path = path;
	initialize(path);
}

void PrintResponse(const dealer::resp_msg &ret, const RequestInfo *req){
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "---Begin----------------PrintResponse-----------------------");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "ret.finish_status:" << ret.finish_status());
	PrintRequest(req);
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "---End------------------PrintResponse-----------------------");
}
void PrintRequest(const RequestInfo *req){

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "begin--------------RequestInfo-------------------------");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "RequestInfo:{id: " << req->id << " status: " << (int)req->status << " time: " << req->time << " manager: " << req->manager <<
		" login: " << req->login << " group: " << req->group << " balance: " << req->balance << " credit: " << req->credit << " prices[0]: " << req->prices[0] << " prices[1]: " <<
		req->prices[1] << " trade:{type: " << (int)req->trade.type << " flags: " << (int)req->trade.flags << " cmd: " << req->trade.cmd << " order: " << req->trade.order <<
		" orderby: " << req->trade.orderby << " symbol: " << req->trade.symbol << " volume: " << req->trade.volume << " price: " << req->trade.price << " sl: " << req->trade.sl << " tp: " <<
		req->trade.tp << " ie_deviation: " << req->trade.ie_deviation << " comment: " << req->trade.comment <<
		" expiration: " << req->trade.expiration << " crc: " << req->trade.crc << "} gw_volume: " << req->gw_volume << " gw_order: " << req->gw_order << " gw_price: " << req->gw_price << " }");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "end--------------RequestInfo--------------------------");
}

void PrintTradeInfo(const TradeTransInfo *trade){
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "begin--------------TradeTransInfo-------------------------");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "TradeTransInfo:{type: " << (int)trade->type << " flags: " << (int)trade->flags << " cmd: " <<
		trade->cmd << " order: " << trade->order << " orderby: " << trade->orderby << " symbol: " << trade->symbol << " volume: " << trade->volume << " price: " <<
		trade->price << " sl: " << trade->sl << " tp: " << trade->tp << " ie_deviation: " << trade->ie_deviation << " comment: " << trade->comment << " expiration: " << trade->expiration << " crc: " << trade->crc << "}");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "end--------------TradeTransInfo--------------------------");
}

void PrintRecord(const TradeRecord &record){
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "---begin----------------TradeRecord-----------------------");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "record:{ " <<
		" order: " << record.order << " login:" << record.login << " symbol:" << record.symbol <<
		" digits:" << record.digits << " cmd:" << record.cmd << " volume:" << record.volume << " open_time: " << record.open_time <<
		" state: " << record.state << " open_price: " << record.open_price << " sl: " << record.sl << " tp: " << record.tp <<
		" close_time: " << record.close_time << " gw_volume: " << record.gw_volume << " expiration: " << record.expiration <<
		" reason: " << record.reason << " conv_reserv: " << record.conv_reserv << " conv_rates[0]: " << record.conv_rates[0] <<
		" conv_rates[1]: " << record.conv_rates[1] << " commission: " << record.commission << " commission_agent: " << record.commission_agent <<
		" storage: " << record.storage << " close_price: " << record.close_price << " profit: " << record.profit << " taxes: " << record.taxes <<
		" magic: " << record.magic << " comment: " << record.comment << " gw_order: " << record.gw_order << "activation: " << record.activation <<
		" gw_open_price: " << record.gw_open_price << " gw_close_price: " << record.gw_close_price << " margin_rate:" << record.margin_rate <<
		" timestamp: " << record.timestamp << " api_data[0]: " << record.api_data[0] << " api_data[1]: " << record.api_data[1] <<
		" api_data[2]: " << record.api_data[2] << " api_data[3]: " << record.api_data[3] << "}");
	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "---end----------------TradeRecord------------------------");
}