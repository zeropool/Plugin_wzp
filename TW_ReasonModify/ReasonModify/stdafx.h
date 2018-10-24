// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  从 Windows 头文件中排除极少使用的信息
// Windows 头文件: 
#include <windows.h>



// TODO:  在此处引用程序需要的其他头文件
//--- exclude rarely-used stuff from Windows headers
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
using namespace std;
//---
#include "mt4serverapi.h" //Mt4 header file. add by wzp 2018.05.28
#include "configuration.h"
//--- macros
#define TERMINATE_STR(str) str[sizeof(str)-1]=0;
#define COPY_STR(dst,src) { strncpy(dst,src,sizeof(dst)-1); dst[sizeof(dst)-1]=0; }
//+------------------------------------------------------------------+