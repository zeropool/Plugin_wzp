#pragma once
#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/property.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/initializer.h>
#include <string.h>
#include <iostream>
#include <windows.h>
using namespace log4cplus;
using namespace std;
const int LOOP_COUNT = 20000;

class DealerLog{
private:
	SharedFileAppenderPtr m_appender;
	//Logger _Logger;
	string _path;
	DealerLog(const string &path);
	void initialize(const string &strPath);
public:
	Logger m_Logger;
	static DealerLog *m_DealerLog;
	static DealerLog *GetInstance(const string &path = "");
};