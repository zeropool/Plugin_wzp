/**************************************************************************** 
*   ����:  2014-12-5 
*   Ŀ��:  ��ȡ�����ļ�����Ϣ����map����ʽ���� 
*   Ҫ��:  �����ļ��ĸ�ʽ����#��Ϊ��ע�ͣ����õ���ʽ��key = value���м���п� 
��Ҳ��û�пո� 
*****************************************************************************/  
  
#ifndef _GET_CONFIG_H_  
#define _GET_CONFIG_H_  
#define COMMENT_CHAR '#'  
#include <string>  
#include <map>  
  
using namespace std;  
  
bool ReadConfig(const string & filename, map<string, string> & m);  
void PrintConfig(const map<string, string> & m);  
  
#endif 