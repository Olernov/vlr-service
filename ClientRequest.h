#pragma once
#include <stdint.h>
#include <string>
#include <map>
#include "ps_common.h"
#include "pspacket.h"

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
	uint64_t imsi;
	int socket;
    int8_t resultCode;
	SubscriberState subscriberState;
	SubscriberOnline subscriberOnline; 
	uint64_t vlrAddress; 
	std::string resultDescr;
};
