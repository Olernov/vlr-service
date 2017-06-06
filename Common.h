#pragma once
#ifdef _WIN32
	#include <Winsock2.h>
	#include <windows.h>
#endif
#include <string>
#include <stdint.h>

#define MAX_THREADS							16
#define MAX_DMS_RESPONSE_LEN				1000
#define ERROR_INIT_PARAMS					-1
#define INIT_FAIL							-2
#define NETWORK_ERROR						-3
#define BAD_DEVICE_RESPONSE					-4
#define NO_CONNECTION_TO_VLR				-5
#define BAD_CLIENT_REQUEST					-6
#define CMD_NOTEXECUTED						-10
#define CMD_UNKNOWN							-30
#define EXCEPTION_CAUGHT					-999
#define OPERATION_SUCCESS					0
#define INFO_NOT_COMPLETE					1
#define ALL_CONNECTIONS_BUSY				-33333

#define SOCKET_TIMEOUT_SEC					10



#define STR_TERMINATOR				'\0'
#define CR_CHAR_CODE				'\r'
#define LF_CHAR_CODE				'\n'
#define HLR_PROMPT					"\x03<"
#define TERMINAL_TYPE				"vt100"

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


enum RequestType {
	stateQuery = 0,
	resetRequest = 1
};

enum SubscriberState
{
	notConnected = 0,
	connected = 1,
	roaming = 2
};

enum SubscriberOnline
{
	offline = 0,
	online = 1	
};


struct ClientRequest
{
	ClientRequest(int socket) : socket(socket), vlrAddress(0) {}

	RequestType requestType;
	uint32_t requestNum;
	uint64_t imsi;
	int socket;
	int8_t resultCode;
	SubscriberState subscriberState;
	SubscriberOnline subscriberOnline; 
	uint64_t vlrAddress; 
	std::string resultDescr;
};
