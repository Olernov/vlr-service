#pragma once
#include "Common.h"
#include <string>
#include "stdint.h"

enum RequestedDevice {
	VLR,
	HLR
};

enum ParseRes {
	success,
	failure
};


/* Class for parsing VLR and HLR successful responses.
	or
		"MT MOBILE SUBSCRIBER STATE
	
	HLR response for HGSDP command looks like this: 	
	or
		"HLR SUBSCRIBER DATA
class ResponseParser
{
public:
	ResponseParser();
	~ResponseParser();

	static ParseRes ResponseParser::Parse(RequestedDevice requestedDevice, const char* response, uint64_t imsi,
		const std::string& homeVlrGt, std::string& result);
	static void ResponseParser::StripHLRResponse(char* start, std::string& result);
private:
	static ParseRes ParseVLRResponse(const char* response, std::string& result);
	static ParseRes ParseHLRResponse(const char* response, uint64_t imsi, const std::string& homeVlrGt, std::string& result);
	static bool AllCharsAreDigits(const char* str, size_t len);
	static bool TryToSkipSubstring(const char* substr, const char*& str);
	static bool TryToSkipDelimiters(const char*& str, const char* end);
	static ParseRes ParseVlrAddr(const char* response, const std::string& homeVlrGt, std::string& result);
};

