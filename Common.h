#pragma once

#include <string>

#define MAX_THREADS							16
#define MAX_DMS_RESPONSE_LEN				1000
#define ERROR_INIT_PARAMS					-1
#define INIT_FAIL							-2
#define NETWORK_ERROR						-3
#define BAD_DEVICE_RESPONSE					-4
#define NO_CONNECTION_TO_VLR				-5
#define CMD_NOTEXECUTED						-10
#define CMD_UNKNOWN							-30
#define EXCEPTION_CAUGHT					-999
#define OPERATION_SUCCESS					0
#define ALL_CONNECTIONS_BUSY				-33333

#define SOCKET_TIMEOUT_SEC					10

#define NUM_OF_EXECUTE_COMMAND_PARAMS		1

#define STR_TERMINATOR				'\0'
#define CR_CHAR_CODE				'\r'
#define LF_CHAR_CODE				'\n'

const int mainThreadIndex = -1;
const std::string crlf = "\n";

#if defined(_WIN32) || defined(_WIN64) 
	#define vsnprintf _vsnprintf 
	#define strcasecmp _stricmp 
	#define strncasecmp _strnicmp 
	#define localtime_r(time_t, tm) localtime_s(tm, time_t)
	#define snprintf sprintf_s
#endif

#ifndef _WIN32
	#define WSAGetLastError() errno
#endif