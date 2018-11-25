#include "get_config.h"
#include <iostream>
#include <windows.h>
#include "log4_cplus.h"

using namespace std;

struct Gap{
	short open_hour, open_min;
	short close_hour, close_min;
};

struct Session{
	Gap gap[7][3];
};

template <class K, class V> struct MutexMap{
	map<K, V> m_queue;
//	mutex m_mutex;

	void Add(const K &key, const V &value){
		//m_mutex.lock();
		m_queue[key] = value;
	//	m_mutex.unlock();
		//	ExtLogger.Out("m_queue[key].type:%d,m_queue[key].point:%llf,m_queue[key].digits:%d", m_queue[key].type, m_queue[key].point, m_queue[key].digits);
	};

	bool Get(const K &key, V &value){
		bool flag = false;
	//	m_mutex.lock();
		if (m_queue.find(key) != m_queue.end()){
			value = m_queue[key];
			flag = true;
		}
	//	m_mutex.unlock();
		return flag;
	};

	void Delete(const K &key){
	//	m_mutex.lock();
		if (m_queue.find(key) != m_queue.end()){
			m_queue.erase(key);
		}
	//	m_mutex.unlock();
	}

	// add empty() judge for MutexMap add by wzp.
	bool Empty(){
	//	m_mutex.lock();
		bool flag = m_queue.empty();
	//	m_mutex.unlock();

		return flag;
	}
};

static map<string, string>		     m_config;
CManagerFactory						 *m_factory;
string								 m_path;
//manager interface 
CManagerInterface				*m_ExtManager_src = NULL;
CManagerInterface				*m_ExtManager_dest = NULL;

MutexMap<string, ConSymbol>		m_Symbol_src;
MutexMap<string, ConSymbol>		m_Symbol_dest;

MutexMap<string, ConGroup>		m_Group_src;
MutexMap<string, ConGroup>		m_Group_dest;
MutexMap<int, ConSymbolGroup>   m_SymbolGroup_src;
vector<string>					m_groups;

map<string, Session>			m_Session;
vector<string>					m_Symbols;

string GetProgramDir(){
	TCHAR exeFullPath[MAX_PATH]; // Full path  
	GetModuleFileName(NULL, exeFullPath, MAX_PATH);
	string strPath = __TEXT("");
	strPath = exeFullPath;    // Get full path of the file  
	int pos = strPath.find_last_of(L'\\', strPath.length());
	return strPath.substr(0, pos);  // Return the directory without the file name 
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

bool AnalysisSession(){
	vector<string>::iterator iter = m_Symbols.begin();

	while (iter != m_Symbols.end()){
		Session ss_tmp{};
		vector<string> vcDiffDay = split(m_config[*iter], "#");
		vector<string>::iterator vc_iter_DiffDay = vcDiffDay.begin();

		while (vc_iter_DiffDay != vcDiffDay.end()){
			vector<string> vcInfo = split(m_config[*iter], "%");
			vector<string> vcDay = split(vcInfo[1], ",");
			vector<string>::iterator vc_iter_Day = vcDay.begin();
			int nInt = 0;

			while (vc_iter_Day != vcDay.end()){
				nInt = atoi(((string)*vc_iter_Day).c_str()) - 1;
				//	ss_tmp.gap[nInt][] = 1;
			}

			vector<string> vcphase = split(vcInfo[0], ",");
			vector<string>::iterator vc_iter_phase = vcphase.begin();

			short arr[3][2][2] = { 0 };
			int cnt = 0;

			while (vc_iter_phase != vcphase.end()){
				vector<string> vc_begin_end = split(*vc_iter_phase, "-");
				vector<string>::iterator vc_iter_begin_end = vc_begin_end.begin();
				int cnt1 = 0;

				while (vc_iter_begin_end != vc_begin_end.end()){
					vector<string> vc_hour_min = split(*vc_iter_begin_end, ":");
					vector<string>::iterator vc_iter_hour_min = vc_hour_min.begin();
					int cnt2 = 0;

					while (vc_iter_hour_min != vc_hour_min.end()){
						arr[cnt][cnt1][cnt2] = atoi(((string)*vc_iter_hour_min).c_str());
						vc_iter_hour_min++;
						cnt2++;
					}

					cnt1++;
					vc_iter_begin_end++;
				}

				cnt++;
				vc_iter_phase++;
			}

			for (int i = 0; i < 3; i++){
				ss_tmp.gap[nInt][i].open_hour = arr[i][0][0];
				ss_tmp.gap[nInt][i].open_min = arr[i][0][1];
				ss_tmp.gap[nInt][i].close_hour = arr[i][1][0];
				ss_tmp.gap[nInt][i].close_min = arr[i][1][1];
			}

			vc_iter_DiffDay++;
		}

		m_Session[*iter] = ss_tmp;
		iter++;
	}

	return true;
}

void AnalysisDay(string data, const string &del, vector<int> &arr){
	vector<string> vc = split(data, del);
	vector<string>::iterator vc_iter = vc.begin();

	while (vc_iter != vc.end()){
		arr.push_back(atoi(((string)*vc_iter).c_str())-1);
		vc_iter++;
	}
}

void AnalysisTime(string data, const string &del, int index[3], short reslut[3][2][2]){
	vector<string> vc = split(data, del);
	vector<string>::iterator vc_iter = vc.begin();
	int nIndex = 0;

	while (vc_iter != vc.end()){
		if ("," == del){
			index[0] = nIndex;
			AnalysisTime(*vc_iter, "-", index, reslut);
		} else if ("-" == del){
			index[1] = nIndex;
			AnalysisTime(*vc_iter, ":", index, reslut);
		} else if (":" == del){
			index[2] = nIndex;
			reslut[index[0]][index[1]][index[2]] = atoi(((string)*vc_iter).c_str());
			//return;
		}

		nIndex++;
		vc_iter++;
	}
}

void AnalysisCirle(string data, const string &del, Session &reslut){
	vector<string> vc = split(data, del);
	vector<string>::iterator vc_iter = vc.begin();

	while (vc_iter != vc.end()){
		if ("$" == del){
			AnalysisCirle(*vc_iter, "%", reslut);
		} else if ("%" == del){
				int index[3] = {0};
				short arrr[3][2][2] = {0};
				vector<int> vc_day;
				AnalysisTime(*vc_iter, ",", index, arrr);
				vc_iter++;
				AnalysisDay(*vc_iter, ",", vc_day);
				vector<int>::iterator n_iter = vc_day.begin();

				while (n_iter != vc_day.end()){
					cout << "while (n_iter != vc_day.end())" << endl;
					for (int i = 0; i < 3;i++){
						reslut.gap[*n_iter][i].open_hour = arrr[i][0][0];
						reslut.gap[*n_iter][i].open_min = arrr[i][0][1];
						reslut.gap[*n_iter][i].close_hour = arrr[i][1][0];
						reslut.gap[*n_iter][i].close_min = arrr[i][1][1];
					}

					n_iter++;
				}
		}

		vc_iter++;
	}
}


void AnalysisSessionTime(map<string, Session> &session,  vector<string> &m_Symbols){
	vector<string>::iterator iter = m_Symbols.begin();

	while (iter != m_Symbols.end()){
		Session ss_tmp{};
		AnalysisCirle(m_config[*iter], "$", ss_tmp);
		session[*iter] = ss_tmp;
		iter++;
	}
}

void printInfo(){
	map<string, Session>::iterator iter = m_Session.begin();

	while (iter != m_Session.end()){
		cout << "symbol:" << iter->first << " Session:" \
			<< iter->second.gap[1][0].open_hour << ":" << iter->second.gap[1][0].open_min << "-" << iter->second.gap[1][0].close_hour << ":" << iter->second.gap[1][0].close_min \
			<<" "<< iter->second.gap[1][1].open_hour << ":" << iter->second.gap[1][1].open_min << "-" << iter->second.gap[1][1].close_hour << ":" << iter->second.gap[1][1].close_min << endl;
		cout << "symbol:" << iter->first << " Session:" \
			<< iter->second.gap[5][0].open_hour << ":" << iter->second.gap[5][0].open_min << "-" << iter->second.gap[5][0].close_hour << ":" << iter->second.gap[5][0].close_min \
			<< " " << iter->second.gap[5][1].open_hour << ":" << iter->second.gap[5][1].open_min << "-" << iter->second.gap[5][1].close_hour << ":" << iter->second.gap[5][1].close_min << endl;

		iter++;
	}
}

bool LoadConfig(){
	m_path = GetProgramDir();

	if (!ReadConfig(m_path + "/" + "tool.cfg", m_config)){
		cout << "if (!ReadConfig" << std::endl;
		return false;
	}

	PrintConfig(m_config);
	string tmp = m_path + "/" + m_config["log_file"];
	DealerLog::GetInstance(tmp + ".log");
	m_groups = split(m_config["groups"], ";");
	m_Symbols = split(m_config["Symbols"], ";");
//	AnalysisSession();
	AnalysisSessionTime(m_Session, m_Symbols);
	printInfo();

	return true;
}

bool CreateMT4LinkInterface(CManagerInterface ** m_interface, const string &server, const int &login, const string &passwd)
{
	//begin connect MT4 server 
	int res = 0;
	int cnt = 0;
	char tmp[256] = { 0 };

	while (cnt < 3){
		if (RET_OK == (*m_interface)->Ping()){
		//	LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "RET_OK == (*m_interface)->Ping()");
			cout <<"if (RET_OK == (*m_interface)->Ping())" << endl;
			return true;
		}

		res = (*m_interface)->Connect(server.c_str());

		if (RET_OK != res){
			sprintf(tmp, "ERR:Connect MT4: connect failed. %s\n", (*m_interface)->ErrorDescription(res));
		//	LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Connect MT4: connect failed. errcode:" << (*m_interface)->ErrorDescription(res));
		//	OutputDebugString(tmp);
			cout << tmp << endl;
			Sleep(1);
		} else{
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
			cout << tmp << endl;
		//	LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "Login MT4: login failed. errcode:" << (*m_interface)->ErrorDescription(res));
			Sleep(1);
		} else{
			break;
		}

		cnt++;
	}

	if (cnt >= 3){
		return false;
	}

	return true;
}

//Create mt4 link include socket link and login add by wzp
bool CreateMT4Link(){
	m_factory = new CManagerFactory(m_config["lib_path"].c_str());

	if (!m_factory->IsValid()){
		cout << "ERR:m_factory.IsValid() false" << endl;
		return false;
	}

	//start init windows socket
	if (RET_OK != m_factory->WinsockStartup()){
		cout << "ERR:m_factory.WinsockStartup() error" << endl;
		return false;
	}

	//pump mode connection src.
	if (NULL == m_ExtManager_src){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:m_ExtManager_src == NULL");
		cout << "if (NULL == m_ExtManager_src)" << endl;
		m_ExtManager_src = m_factory->Create(ManAPIVersion);
	}

	//pump mode connection dest.
	if (NULL == m_ExtManager_dest){
		cout << "if (NULL == m_ExtManager_dest)" << endl;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "INFO:m_ExtManager_dest == NULL");
		m_ExtManager_dest = m_factory->Create(ManAPIVersion);
	}


	if (NULL == m_ExtManager_dest || NULL == m_ExtManager_src){
		cout << "(NULL == m_ExtDealer) || (NULL == m_ExtManagerPump) || NULL == m_pool[0] || NULL == m_pool[1])" << endl;
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "(NULL == m_ExtDealer) || (NULL == m_ExtManagerPump) || NULL == m_pool[0] || NULL == m_pool[1])");
		return false;
	}

    CreateMT4LinkInterface(&m_ExtManager_src, m_config["mt4_addr_src"], atoi(m_config["mt4_login_name_src"].c_str()), m_config["mt4_passwd_src"]);
	CreateMT4LinkInterface(&m_ExtManager_dest, m_config["mt4_addr_dest"], atoi(m_config["mt4_login_name_dest"].c_str()), m_config["mt4_passwd_dest"]);

	return true;
}

//process the symbol info.
bool StaticSymbolConfigInfo(CManagerInterface *m_ExtManager, MutexMap<string, ConSymbol>	&Con){
	int cnt = 0;
	ConSymbol *m_ConSymbol = m_ExtManager->CfgRequestSymbol(&cnt);

	if (m_ConSymbol == NULL){
		cout << "if (m_ConSymbol == NULL)" << endl;
		return false;
	}

	for (int i = 0; i < cnt; i++){
		ConSymbol tmpSymbolConfig{};
		memcpy(&tmpSymbolConfig, &m_ConSymbol[i], sizeof(tmpSymbolConfig));
		cout << m_ConSymbol[i].symbol << endl;
		Con.Add(m_ConSymbol[i].symbol, tmpSymbolConfig);
	}

	m_ExtManager->MemFree(m_ConSymbol);
	m_ConSymbol = NULL;
	return true;
}

bool SetSymbolsSession(CManagerInterface *m_ExtManager, MutexMap<string, ConSymbol>	&Con){
	vector<string>::iterator iter = m_Symbols.begin();
	

	while (iter != m_Symbols.end()){
		ConSymbol tmp{};
		Con.Get(*iter, tmp);
		
		for (int i = 0; i < 7;i++){
			for (int k = 0; k < 3;k++){
				tmp.sessions[i].quote[k].open_hour = m_Session[*iter].gap[i][k].open_hour;
				tmp.sessions[i].quote[k].open_min = m_Session[*iter].gap[i][k].open_min;
				tmp.sessions[i].quote[k].close_hour = m_Session[*iter].gap[i][k].close_hour;
				tmp.sessions[i].quote[k].close_min = m_Session[*iter].gap[i][k].close_min;

				tmp.sessions[i].trade[k].open_hour = m_Session[*iter].gap[i][k].open_hour;
				tmp.sessions[i].trade[k].open_min = m_Session[*iter].gap[i][k].open_min;
				tmp.sessions[i].trade[k].close_hour = m_Session[*iter].gap[i][k].close_hour;
				tmp.sessions[i].trade[k].close_min = m_Session[*iter].gap[i][k].close_min;
			}
		}

		m_ExtManager->CfgUpdateSymbol(&tmp);
		iter++;
	}
	
//	m_ExtManager->CfgUpdateSymbol();
	return true;
}

bool SetSymbolsSession2(CManagerInterface *m_ExtManager, MutexMap<string, ConSymbol>	&Con){
	vector<string>::iterator iter = m_Symbols.begin();


	while (iter != m_Symbols.end()){
		ConSymbol tmp{};
		map<string, ConSymbol>::iterator m_iter = Con.m_queue.begin();

		while (m_iter != Con.m_queue.end()){
			string Str_tmp = m_iter->first;
			//Str_tmp.substr()
			if (string::npos != Str_tmp.find(*iter)){
				cout << "Str_tmp:" << Str_tmp << "*iter:" << *iter << endl;
				memcpy(&tmp, &(ConSymbol)m_iter->second, sizeof(ConSymbol));

				for (int i = 0; i < 7; i++){
					for (int k = 0; k < 3; k++){
						tmp.sessions[i].quote[k].open_hour = m_Session[*iter].gap[i][k].open_hour;
						tmp.sessions[i].quote[k].open_min = m_Session[*iter].gap[i][k].open_min;
						tmp.sessions[i].quote[k].close_hour = m_Session[*iter].gap[i][k].close_hour;
						tmp.sessions[i].quote[k].close_min = m_Session[*iter].gap[i][k].close_min;

						tmp.sessions[i].trade[k].open_hour = m_Session[*iter].gap[i][k].open_hour;
						tmp.sessions[i].trade[k].open_min = m_Session[*iter].gap[i][k].open_min;
						tmp.sessions[i].trade[k].close_hour = m_Session[*iter].gap[i][k].close_hour;
						tmp.sessions[i].trade[k].close_min = m_Session[*iter].gap[i][k].close_min;
					}
				}

				m_ExtManager->CfgUpdateSymbol(&tmp);
			}

			m_iter++;
		}


		iter++;
	}

	//	m_ExtManager->CfgUpdateSymbol();
	return true;
}

void update_config(CManagerInterface *m_ExtManager, ConSymbol con_dest, ConSymbol con_src){
	if (m_config["update_enable"] != "OK"){
		cout << "not ok" << endl;
		return;
	}

	if (con_dest.swap_long == con_src.swap_long&&
		con_dest.swap_short == con_src.swap_short&&
		con_dest.swap_type == con_src.swap_type){
		return;
	}

	con_dest.swap_long = con_src.swap_long;
	con_dest.swap_short = con_src.swap_short;
	con_dest.swap_type = con_src.swap_type;

	int ret = m_ExtManager->CfgUpdateSymbol(&con_dest);

	if (ret != RET_OK){
		cout <<"CfgUpdateSymbol failed!"<<endl;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "CfgUpdateSymbol failed!");
	}
}

void Compare(){
	MutexMap<string, ConSymbol> m_Con_src;
	MutexMap<string, ConSymbol> m_Con_dest;
	bool bFlag = true;
	if (m_Symbol_src.m_queue.size() >= m_Symbol_dest.m_queue.size()){
		m_Con_src = m_Symbol_src;
		m_Con_dest = m_Symbol_dest;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_Con_src=" << "m_Symbol_src; m_Con_dest="<<"m_Symbol_dest");
	} else{
		m_Con_src = m_Symbol_dest;
		m_Con_dest = m_Symbol_src;
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_Con_src=" << "m_Symbol_dest; m_Con_dest=" << "m_Symbol_src");
	}

	LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_Symbol_src:" << m_Symbol_src.m_queue.size() 
		<< " == " << "m_Symbol_dest:" << m_Symbol_dest.m_queue.size());

	map<string, ConSymbol>::iterator iter = m_Con_src.m_queue.begin();
	ConSymbol tmp{};

	while (iter != m_Con_src.m_queue.end()){
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "begin---------------------");
		if (!m_Con_dest.Get(iter->first, tmp)){
			cout << iter->first << endl;
			LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "m_Con_src:" << iter->first);
		} else{
			
			if (strcmp(iter->second.description, tmp.description)){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " description:" << "  m_Con_src: " << iter->second.description << " m_Con_dest: " << tmp.description);
			}

			if (strcmp(iter->second.source, tmp.source)){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " source:" << "  m_Con_src: " << iter->second.source << " m_Con_dest: " << tmp.source);
			}

			if (strcmp(iter->second.currency, tmp.currency)){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " currency:" << "  m_Con_src: " << iter->second.currency << " m_Con_dest: " << tmp.currency);
			}

			if (iter->second.type != tmp.type){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " type:" << "  m_Con_src: " << iter->second.type << " m_Con_dest: " << tmp.type);
			}

			if (iter->second.digits != tmp.digits){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " digits:" << "  m_Con_src: " << iter->second.digits << " m_Con_dest: " << tmp.digits);
			}

			if (iter->second.trade != tmp.trade){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " trade:" << "  m_Con_src: " << iter->second.trade << " m_Con_dest: " << tmp.trade);
			}

			if (iter->second.background_color != tmp.background_color){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " background_color:" << "  m_Con_src: " << iter->second.background_color << " m_Con_dest: " << tmp.background_color);
			}

			if (iter->second.count != tmp.count){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " count:" << "  m_Con_src: " << iter->second.count << " m_Con_dest: " << tmp.count);
			}

			if (iter->second.count_original != tmp.count_original){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " count_original:" << "  m_Con_src: " << iter->second.count_original << " m_Con_dest: " << tmp.count_original);
			}

			if (memcmp(iter->second.external_unused, tmp.external_unused, sizeof(iter->second.external_unused))){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " external_unused:" << "  m_Con_src: " << iter->second.external_unused << " m_Con_dest: " << tmp.external_unused);
			}

			if (iter->second.realtime != tmp.realtime){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " realtime:" << "  m_Con_src: " << iter->second.realtime << " m_Con_dest:" << tmp.realtime);
			}

			if (iter->second.starting != tmp.starting){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " starting:" << "  m_Con_src: " << iter->second.starting << " m_Con_dest:" << tmp.starting);
			}

			if (iter->second.expiration != tmp.expiration){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " expiration:" << "  m_Con_src: " << iter->second.expiration << " m_Con_dest: " << tmp.expiration);
			}

			if (memcmp(iter->second.sessions, tmp.sessions, sizeof(iter->second.sessions))){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " sessions:" << "  m_Con_src: " << iter->second.sessions << " m_Con_dest: " << tmp.sessions);
			}

			if (iter->second.profit_mode != tmp.profit_mode){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " profit_mode:" << "  m_Con_src:" << iter->second.profit_mode << " m_Con_dest: " << tmp.profit_mode);
			}

			if (iter->second.profit_reserved != tmp.profit_reserved){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " profit_reserved:" << "  m_Con_src: " << iter->second.profit_reserved << " m_Con_dest: " << tmp.profit_reserved);
			}

			if (iter->second.filter != tmp.filter){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " filter:" << "  m_Con_src: " << iter->second.filter << " m_Con_dest: " << tmp.filter);
			}

			if (iter->second.filter_counter != tmp.filter_counter){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " filter_counter:" << "  m_Con_src: " << iter->second.filter_counter << " m_Con_dest: " << tmp.filter_counter);
			}

			if (iter->second.filter_limit != tmp.filter_limit){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " filter_limit:" << "  m_Con_src: " << iter->second.filter_limit << " m_Con_dest: " << tmp.filter_limit);
			}

			if (iter->second.filter_smoothing != tmp.filter_smoothing){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " filter_smoothing:" << "  m_Con_src: " << iter->second.filter_smoothing << " m_Con_dest: " << tmp.filter_smoothing);
			}

			if (iter->second.filter_reserved != tmp.filter_reserved){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " filter_reserved:" << "  m_Con_src: " << iter->second.filter_reserved << " m_Con_dest: " << tmp.filter_reserved);
			}

			if (iter->second.logging != tmp.logging){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " logging:" << "  m_Con_src: " << iter->second.logging << " m_Con_dest: " << tmp.logging);
			}

			if (iter->second.spread != tmp.spread){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " spread:" << "  m_Con_src: " << iter->second.spread << " m_Con_dest: " << tmp.spread);
			}

			if (iter->second.spread_balance != tmp.spread_balance){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " spread_balance:" << "  m_Con_src: " << iter->second.spread_balance << " m_Con_dest: " << tmp.spread_balance);
			}

			if (iter->second.exemode != tmp.exemode){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " exemode:" << "  m_Con_src: " << iter->second.exemode << " m_Con_dest: " << tmp.exemode);
			}

			if (iter->second.swap_enable != tmp.swap_enable){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_enable:" << "  m_Con_src: " << iter->second.swap_enable << " m_Con_dest: " << tmp.swap_enable);
			}

			if (iter->second.swap_type != tmp.swap_type){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_type:" << "  m_Con_src: " << iter->second.swap_type << " m_Con_dest: " << tmp.swap_type);
			}

			if (iter->second.swap_long != tmp.swap_long){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_long:" << "  m_Con_src: " << iter->second.swap_long << " m_Con_dest: " << tmp.swap_long);
			}

			if (iter->second.swap_short != tmp.swap_short){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_short:" << "  m_Con_src: " << iter->second.swap_short << " m_Con_dest: " << tmp.swap_short);
			}

			if (iter->second.swap_rollover3days != tmp.swap_rollover3days){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_rollover3days:" << "  m_Con_src: " << iter->second.swap_rollover3days << " m_Con_dest: " << tmp.swap_rollover3days);
			}

			if (iter->second.contract_size != tmp.contract_size){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " contract_size:" << "  m_Con_src: " << iter->second.contract_size << " m_Con_dest: " << tmp.contract_size);
			}

			if (iter->second.tick_value != tmp.tick_value){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " tick_value:" << "  m_Con_src: " << iter->second.tick_value << " m_Con_dest: " << tmp.tick_value);
			}

			if (iter->second.tick_size != tmp.tick_size){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " tick_size:" << "  m_Con_src: " << iter->second.tick_size << " m_Con_dest: " << tmp.tick_size);
			}

			if (iter->second.stops_level != tmp.stops_level){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " stops_level:" << "  m_Con_src: " << iter->second.stops_level << " m_Con_dest: " << tmp.stops_level);
			}

			if (iter->second.gtc_pendings != tmp.gtc_pendings){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " gtc_pendings:" << "  m_Con_src: " << iter->second.gtc_pendings << " m_Con_dest: " << tmp.gtc_pendings);
			}

			if (iter->second.margin_mode != tmp.margin_mode){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_mode:" << "  m_Con_src: " << iter->second.margin_mode << " m_Con_dest: " << tmp.margin_mode);
			}

			if (iter->second.margin_initial != tmp.margin_initial){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_initial:" << "  m_Con_src: " << iter->second.margin_initial << " m_Con_dest: " << tmp.margin_initial);
			}

			if (iter->second.margin_maintenance != tmp.margin_maintenance){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_maintenance:" << "  m_Con_src: " << iter->second.margin_maintenance << " m_Con_dest: " << tmp.margin_maintenance);
			}

			if (iter->second.margin_hedged != tmp.margin_hedged){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_hedged:" << "  m_Con_src: " << iter->second.margin_hedged << " m_Con_dest: " << tmp.margin_hedged);
			}

			if (iter->second.margin_divider != tmp.margin_divider){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_divider:" << "  m_Con_src: " << iter->second.margin_divider << " m_Con_dest: " << tmp.margin_divider);
			}

			if (iter->second.point != tmp.point){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " point:" << "  m_Con_src: " << iter->second.point << " m_Con_dest: " << tmp.point);
			}

			if (iter->second.multiply != tmp.multiply){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " multiply:" << "  m_Con_src: " << iter->second.multiply << " m_Con_dest: " << tmp.multiply);
			}

			if (iter->second.bid_tickvalue != tmp.bid_tickvalue){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " bid_tickvalue:" << "  m_Con_src: " << iter->second.bid_tickvalue << " m_Con_dest: " << tmp.bid_tickvalue);
			}

			if (iter->second.ask_tickvalue != tmp.ask_tickvalue){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " ask_tickvalue:" << "  m_Con_src: " << iter->second.ask_tickvalue << " m_Con_dest: " << tmp.ask_tickvalue);
			}

			if (iter->second.long_only != tmp.long_only){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " long_only:" << "  m_Con_src: " << iter->second.long_only << " m_Con_dest: " << tmp.long_only);
			}

			if (iter->second.instant_max_volume != tmp.instant_max_volume){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " instant_max_volume:" << "  m_Con_src: " << iter->second.instant_max_volume << " m_Con_dest: " << tmp.instant_max_volume);
			}

			if (memcmp(iter->second.margin_currency, tmp.margin_currency, sizeof(iter->second.margin_currency))){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_currency:" << "  m_Con_src: " << iter->second.margin_currency << " m_Con_dest: " << tmp.margin_currency);
			}

			if (iter->second.freeze_level != tmp.freeze_level){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " freeze_level:" << "  m_Con_src: " << iter->second.freeze_level << " m_Con_dest: " << tmp.freeze_level);
			}

			if (iter->second.margin_hedged_strong != tmp.margin_hedged_strong){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " margin_hedged_strong:" << "  m_Con_src: " << iter->second.margin_hedged_strong << " m_Con_dest: " << tmp.margin_hedged_strong);
			}

			if (iter->second.value_date != tmp.value_date){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " value_date:" << "  m_Con_src: " << iter->second.value_date << " m_Con_dest: " << tmp.value_date);
			}

			if (iter->second.quotes_delay != tmp.quotes_delay){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " quotes_delay:" << "  m_Con_src: " << iter->second.quotes_delay << " m_Con_dest: " << tmp.quotes_delay);
			}

			if (iter->second.swap_openprice != tmp.swap_openprice){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_openprice:" << "  m_Con_src: " << iter->second.swap_openprice << " m_Con_dest: " << tmp.swap_openprice);
			}

			if (iter->second.swap_variation_margin != tmp.swap_variation_margin){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " swap_variation_margin:" << "  m_Con_src: " << iter->second.swap_variation_margin << " m_Con_dest: " << tmp.swap_variation_margin);
			}

			if (memcmp(iter->second.unused, tmp.unused, sizeof(iter->second.unused))){
				bFlag = false;
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, iter->first << " unused:" << "  m_Con_src: " << iter->second.unused << " m_Con_dest: " << tmp.unused);
			}

			if (!bFlag){
				update_config(m_ExtManager_dest, m_Symbol_dest.m_queue[iter->first], m_Symbol_src.m_queue[iter->first]);
			}
		}
		LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, "end---------------------");
		memset(&tmp,0,sizeof(tmp));
		iter++;
	}
}
//group  setting process 
void __stdcall OnPumpingFunc(int code, int type, void *data, void *param){

}

bool SwitchMode(CManagerInterface *m_ExtManager){
	char tmp[256];
	int res = m_ExtManager->PumpingSwitchEx(OnPumpingFunc, 0, NULL);

	if (RET_OK != res){
		sprintf(tmp, "ERR:PumpingSwitch error ERRCODE:%", m_ExtManager->ErrorDescription(res));
		LOG4CPLUS_ERROR(DealerLog::GetInstance()->m_Logger, "PumpingSwitch error ERRCODE:" << m_ExtManager->ErrorDescription(res));
		OutputDebugString(tmp);
		return false;
	}

	return true;
}

bool StaticGroupConfigInfo(CManagerInterface	*m_ExtManager, MutexMap<string, ConGroup> &Con){
	int cnt = 0;
	ConGroup *m_ConGroup = m_ExtManager->CfgRequestGroup(&cnt);

	if (m_ConGroup == NULL){
		cout << "if (m_ConSymbol == NULL)" << endl;
		return false;
	}

	for (int i = 0; i < cnt; i++){
		ConGroup tmpSymbolConfig{};
		memcpy(&tmpSymbolConfig, &m_ConGroup[i], sizeof(tmpSymbolConfig));
		cout << m_ConGroup[i].group << endl;
		Con.Add(m_ConGroup[i].group, tmpSymbolConfig);
	}

	m_ExtManager->MemFree(m_ConGroup);
	m_ConGroup = NULL;
	return true;
}

bool StaticSymbolGroupConfigInfo(CManagerInterface	*m_ExtManager, MutexMap<int, ConSymbolGroup> &Con){
	ConSymbolGroup symbolGrp[MAX_SEC_GROUPS] = {0};
	int ret = m_ExtManager->CfgRequestSymbolGroup(symbolGrp);

	if (symbolGrp == NULL){
		cout << "if (m_ConSymbol == NULL)" << endl;
		return false;
	}

	for (int i = 0; i < MAX_SEC_GROUPS; i++){
		ConSymbolGroup tmpSymbolConfig{};
		memcpy(&tmpSymbolConfig, &symbolGrp[i], sizeof(tmpSymbolConfig));
		cout <<"i:"<<i<< symbolGrp[i].name << endl;
		Con.Add(i, tmpSymbolConfig);
	}

	//m_ExtManager->MemFree(symbolGrp);
	//symbolGrp = NULL;
	return true;
}

void SetGroup(CManagerInterface	*m_ExtManager){
	//conTmpConGroup conTmp{};
	vector<string>::iterator iter = m_groups.begin();

	while (iter != m_groups.end()){
		cout << *iter << endl;
		ConGroup conTmp{};
		m_Group_src.Get(*iter, conTmp);
	//	conTmp.secgroups[]

		for (int i = 0; i < MAX_SEC_GROUPS; i++){
			ConSymbolGroup sy{};
			m_SymbolGroup_src.Get(i, sy); 

			if (conTmp.secgroups[i].execution != EXECUTION_MANUAL || !conTmp.secgroups[i].show || !conTmp.secgroups[i].trade){
				//cout << sy.name << ":" << conTmp.secgroups[i].spread_diff << endl;
				continue;
			} else{
				LOG4CPLUS_INFO(DealerLog::GetInstance()->m_Logger, conTmp.group<<":"<<sy.name);
				conTmp.secgroups[i].execution = EXECUTION_AUTO;
				cout << conTmp.group << ":" << sy.name << endl;
			}
		}

		m_ExtManager->CfgUpdateGroup(&conTmp);
		iter++;
	}
}

int main(int argc, char *argv[]){
	if (!LoadConfig()){
		cout << "if (!LoadConfig()){" << endl;
		//return 0;
	}
	//create mt4 link:Note good
	if (!CreateMT4Link()){
		cout << "if (!CreateMT4Link()){" << endl;
		return 0;
	}

	if (!StaticSymbolConfigInfo(m_ExtManager_src, m_Symbol_src)){
		cout << "if (!StaticSymbolConfigInfo(m_ExtManager_src, m_Symbol_src))" << endl;
		return 0;
	}

	if (!StaticSymbolConfigInfo(m_ExtManager_dest, m_Symbol_dest)){
		cout << "if (!StaticSymbolConfigInfo(m_ExtManager_src, m_Symbol_src))" << endl;
		return 0;
	}

	Compare();

	//if (!StaticGroupConfigInfo(m_ExtManager_src, m_Group_src)){
	//	cout << "if (!StaticSymbolConfigInfo(m_ExtManager_dest, m_Symbol_dest))" << endl;
	//	return 0;
	//}

	//if (!StaticSymbolGroupConfigInfo(m_ExtManager_src, m_SymbolGroup_src)){
	//	cout << "if (!StaticSymbolGroupConfigInfo(m_ExtManager_dest, m_Symbol_dest))" << endl;
	//	return 0;
	//}


	//SetGroup(m_ExtManager_src);
	//if (m_config["Enable"] == "YES"){
	//	SetSymbolsSession2(m_ExtManager_src, m_Symbol_src);
	//} else{
	//	SetSymbolsSession(m_ExtManager_src, m_Symbol_src);
	//}
	
	
	cout <<"suc!" <<endl;
	//system("pause");
//	exit(0);
	return 0;
}