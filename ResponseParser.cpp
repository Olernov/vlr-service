#include "ResponseParser.h"


ResponseParser::ResponseParser()
{
}


ResponseParser::~ResponseParser()
{
}


ParseRes ResponseParser::Parse(RequestedDevice requestedDevice, const char* response, ClientRequest& clientRequest)
{
	if (requestedDevice == VLR) {
		return ParseVLRResponse(response, clientRequest);
	}
	else if (requestedDevice == HLR) {
		return ParseHLRResponse(response, clientRequest);
	}
	else {
		return failure;
	}
}


ParseRes ResponseParser::ParseVLRResponse(const char* response, ClientRequest& clientRequest)
{
	const char* end = response + strlen(response) - 1;
	if (!TryToSkipSubstring("SUBSCRIBER DETAILS", response)) {
		return failure;
	}
	if (!TryToSkipSubstring("IMSI             MSISDN           STATE    RESTR    LAI", response)) {
		return failure;
	}
	if (!TryToSkipDelimiters(response, end)) {
		return failure;
	}
	
	// skip IMSI
	response += strcspn(response, " \t\r\n");
	if (!TryToSkipDelimiters(response, end)) {
		return failure;
	}
	size_t SecondFieldLen = strcspn(response, " \t\r\n");
	std::string result;
	// There may be either MSISDN or STATE in 2nd field. If there are digits only, we
	// suppose this is MSISDN, otherwise we consider it as STATE
	if (AllCharsAreDigits(response, SecondFieldLen)) {
		// skip 2nd field (probably MSISDN)
		response += SecondFieldLen;
		if (!TryToSkipDelimiters(response, end)) {
			return failure;
		}
		size_t ThirdFieldLen = strcspn(response, " \t\r\n");
		result.resize(ThirdFieldLen);
		std::copy(response, response + ThirdFieldLen, result.begin());
	}
	else {
		result.resize(SecondFieldLen);
		std::copy(response, response + SecondFieldLen, result.begin());		
	}
	
	if (!result.compare("NOTREG")) {
		return infoNotComplete;
	}
	if (!result.compare("IDLE")) {
		clientRequest.subscriberState = connected;
		clientRequest.subscriberOnline = offline;
		return success;
	}
	else if(!result.compare("BUSY")) {
		clientRequest.subscriberState = connected;
		clientRequest.subscriberOnline = online;
		return success;
	}
	else if(!result.compare("DET") || !result.compare("IDET")) {
		clientRequest.subscriberState = notConnected;
		clientRequest.subscriberOnline = offline;
		return success;
	}
	else {
		clientRequest.resultDescr = "Unknown subscriber state in VLR: " + result;
		return failure;
	}
}


ParseRes ResponseParser::ParseHLRResponse(const char* response, ClientRequest& clientRequest)
{
	const char* end = response + strlen(response) - 1;
	if (!TryToSkipSubstring("HLR SUBSCRIBER DATA", response)) {
		clientRequest.resultDescr = "HLR SUBSCRIBER DATA not found in HLR response";
		return failure;
	}
	if (!TryToSkipSubstring("MSISDN           IMSI             STATE          AUTHD", response)) {
		clientRequest.resultDescr = "Unknown format of HLR response";
		return failure;
	}
	if (!TryToSkipDelimiters(response, end)) {
		clientRequest.resultDescr = "Unknown format of HLR response";
		return failure;
	}
	// try to find IMSI in next line of response and skip it. 
	// Next field after IMSI should be STATE
	char imsiStr[20];
	snprintf(imsiStr, sizeof(imsiStr), "%llu", clientRequest.imsi);
	if (!TryToSkipSubstring(imsiStr, response)) {
		clientRequest.resultDescr = "Unknown format of HLR response";
		return failure;
	}

	// One of states: CONNECTED or NOT CONNECTED should be in response.
	// Otherwise return failure.
	//std::string result;
	if (strstr(response, "NOT CONNECTED")) {
		clientRequest.subscriberState = notConnected;
		return success;
	}
	if (!strstr(response, "CONNECTED")) {
		clientRequest.resultDescr = "Unknown subscriber state in HLR";
		return failure;
	}
	if (!TryToSkipSubstring("LOCATION DATA", response)) {
		clientRequest.resultDescr = "LOCATION DATA not found in HLR response";
		return failure;
	}
	if (!TryToSkipSubstring("VLR ADDRESS       MSRN            MSC NUMBER          LMSID", response)) {
		clientRequest.resultDescr = "VLR address not found in HLR response";
		return failure;
	}
	if (!TryToSkipDelimiters(response, end)) {
		clientRequest.resultDescr = "Unknown format of HLR response";
		return failure;
	}
	return ParseVlrAddr(response, clientRequest);
}


bool ResponseParser::AllCharsAreDigits(const char* str, size_t len)
{
	const char* p = str;
	while (p < str + len) {
		if (!(*p >= '0' && *p <='9')) {
			return false;
		}
		p++;
	}
	return true;
}


bool ResponseParser::TryToSkipSubstring(const char* substr, const char*& str)
{
	const char* p = strstr(str, substr);
	if (!p) {
		return false;
	}
	str = p + strlen(substr);
	return true;
}


bool ResponseParser::TryToSkipDelimiters(const char*& str, const char* end)
{
	str += strspn(str, " ;\r\n");
	if (str >= end) {
		return false;
	}
	return true;
}


ParseRes ResponseParser::ParseVlrAddr(const char* response, ClientRequest& clientRequest)
{
	int vlrLength = strcspn(response, " \t\r\n");
	char vlr[30];
    strncpy(vlr, response, vlrLength);
	vlr[sizeof(vlr) - 1] = STR_TERMINATOR;

	/*	Visitor Location Register (VLR) address
	 Expressed as na-ai, UNKNOWN, RESTRICTED or BARRED where:
	 na Nature of address
	 3 National
	 4 International
	 ai Address information
	 UNKNOWN Location unknown
	 RESTRICTED Location restricted
	 BARRED Location barred */
	if (!strcmp(vlr, "UNKNOWN") || !strcmp(vlr, "RESTRICTED") || !strcmp(vlr, "BARRED")) {
		clientRequest.subscriberState = notConnected;
		return success;
	}
	
	const char* vlrAddr = strchr(vlr, '-');
	if (!vlrAddr) {
		clientRequest.resultDescr = std::string("Unable to parse VLR address: ") + vlr;
		return failure;
	}
	vlrAddr++;
	clientRequest.subscriberState = roaming;
	clientRequest.vlrAddress = strtoull(vlrAddr, nullptr, 10);
	return success;
}


void ResponseParser::StripHLRResponse(char* start, std::string& result)
{
	start += strspn(start, " ;\r\n");
	// remove spaces, \r and \n at the end of string
	char* responseEnd = start + strlen(start) - 1;
	char* end = responseEnd;
	while (end > start && (*end == ' ' || *end == '\r' || *end == '\n' || *end == '\t')) {
		end--;
	}
	if ((end > start) && (end < responseEnd)) {
		*(end + 1) = STR_TERMINATOR;
	}

	// replace \r and \n with spaces 
	for (char* p3 = start; p3 < end; p3++) {
		if (*p3 == '\r' || *p3 == '\n') {
			*p3 = ' ';
		}
	}
	if (!strcmp(start + strlen(start) - 2, HLR_PROMPT)) {
		start[strlen(start) - 2] = STR_TERMINATOR;
	}
	result = start;
}
