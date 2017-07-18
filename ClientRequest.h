#pragma once
#include <stdint.h>
#include <string>
#include <map>
#include <chrono>
#include "ps_common.h"
#include "pspacket.h"

enum RequestType {
	stateQuery = 0,
    resetRequest = 1,
    activateRequest = 2,
    deactivateRequest = 3
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

using namespace std::chrono;

class ClientRequest
{
public:
	ClientRequest(int socket);
	bool ValidateAndSetRequestParams(uint32_t reqNum, const std::multimap<__uint16_t, SPSReqAttrParsed>& requestAttrs,
            std::string& errorDescr);
	bool SendRequestResultToClient(std::string& errorDescr);
	std::string DumpResults();

    RequestType requestType;
	uint32_t requestNum;
    uint64_t subscriberID;
	int socket;
    SubscriberState subscriberState;
	SubscriberOnline subscriberOnline; 
	uint64_t vlrAddress; 
    steady_clock::time_point accepted;
    int8_t resultCode;
	std::string resultDescr;
};
