#pragma once
#include <string>
#include "stdint.h"
#include "Common.h"
#include "ClientRequest.h"

enum RequestedDevice {
	VLR,
	HLR
};

enum ParseRes {
	success,
	failure,
	infoNotComplete
};


/* Class for parsing VLR and HLR successful queries.
VLR responses for MGSSP command looks like this: 	
		"MT MOBILE SUBSCRIBER STATE
		SUBSCRIBER DETAILS
		IMSI             MSISDN           STATE    RESTR    LAI
		250270100520482  NOTREG
		END		"
	or
		"MT MOBILE SUBSCRIBER STATE
		SUBSCRIBER DETAILS
		IMSI             MSISDN           STATE    RESTR    LAI
		250270100293873  79586235045      IDLE              250-27-51208
		CELL ID
		10511
		LAST RADIO ACCESS
		DATE      TIME
		170512    1454
		HLRADD
		79506651020
		..."

	The goal is to get subscriber STATE from this response.
	
	HLR response for HGSDP command looks like this: 	
		"HLR SUBSCRIBER DATA
		SUBSCRIBER IDENTITY
		MSISDN           IMSI             STATE          AUTHD
		79274006414      250270100604804  CONNECTED      AVAILABLE
		NAM
		0
		LOCATION DATA
		VLR ADDRESS       MSRN            MSC NUMBER          LMSID
		4-79270009013                     79270009012        
		SGSN NUMBER
		UNKNOWN
		END "
	or
		"HLR SUBSCRIBER DATA
		SUBSCRIBER IDENTITY
		MSISDN           IMSI             STATE          AUTHD
                 250270100273000  NOT CONNECTED
		END "
	The goal is to get VLR address or subscriber STATE from this response.*/	
class ResponseParser
{
public:
    static ParseRes ParseAndSetResultCode(RequestType requestType, RequestedDevice requestedDevice,
                          const char* response, ClientRequest& request);
    static void StripHLRResponse(char* start, std::string& result);
private:
    static ParseRes Parse(RequestType requestType, RequestedDevice requestedDevice,
                          const char* response, ClientRequest& clientRequest);
    static ParseRes ParseStateQueryVLRResponse(const char* response, ClientRequest& clientRequest);
    static ParseRes ParseStateQueryHLRResponse(const char* response, ClientRequest& clientRequest);
    static ParseRes ParseImsiQueryVLRResponse(const char* response, ClientRequest& clientRequest);
	static bool AllCharsAreDigits(const char* str, size_t len);
	static bool TryToSkipSubstring(const char* substr, const char*& str);
	static bool TryToSkipDelimiters(const char*& str, const char* end);
	static ParseRes ParseVlrAddr(const char* response, ClientRequest& clientRequest);

};
