#pragma once
#ifdef _WIN32
	#include <Winsock2.h>
	#include <windows.h>
#else
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <inttypes.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <ctype.h>
#endif

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <string>

#if defined(__linux__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#endif

#if defined(__linux__)
    #define ntohll(x) be64toh(x)
    #define htonll(x) htobe64(x)
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
    #define sprintf_s snprintf
    #define ioctlsocket ioctl
    #define WSAEWOULDBLOCK EWOULDBLOCK
#endif


#define BAD_DEVICE_RESPONSE					-4
#define NO_CONNECTION_TO_VLR				-5
#define BAD_CLIENT_REQUEST					-6
#define CMD_NOTEXECUTED						-10
#define CMD_UNKNOWN							-30
#define EXCEPTION_CAUGHT					-999
#define OPERATION_SUCCESS					0
#define INFO_NOT_COMPLETE					1

#define STR_TERMINATOR				'\0'
#define HLR_PROMPT                  "\x03<"

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


static const uint16_t VLR_GATEWAY_COMMAND = 0x0500;
static const uint16_t VLR_GATEWAY_SUBSCR_ID = 0x0501;
static const uint16_t VLR_GATEWAY_STATE = 0x0502;
static const uint16_t VLR_GATEWAY_ONLINE_STATUS  = 0x0503;
static const uint16_t VLR_GATEWAY_VLR_ADDRESS = 0x0504;
static const uint16_t VLR_GATEWAY_IMSI = 0x0505;

enum RequestType {
    stateQuery = 0,
    resetRequest = 1,
    activateRequest = 2,
    deactivateRequest = 3,
    imsiQuery = 4
};
