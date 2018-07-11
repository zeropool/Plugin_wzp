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