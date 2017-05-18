#include "ResponseParser.h"


ResponseParser::ResponseParser()
{
}


ResponseParser::~ResponseParser()
{
}


ParseRes ResponseParser::Parse(RequestedDevice requestedDevice, const char* response, uint64_t imsi, 
		const std::string& homeVlrGt, std::string& result)
{
	if (requestedDevice == VLR) {
		return ParseVLRResponse(response, result);
	}
	else if (requestedDevice == HLR) {
		return ParseHLRResponse(response, imsi, homeVlrGt, result);
	}
	else {
		return failure;
	}
}


ParseRes ResponseParser::ParseVLRResponse(const char* response, std::string& result)
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
	// There may be either MSISDN or STATE in 2nd field. If there are digits only, we
	// suppose this is MSISDN, otherwise we consider it as STATE
	if (!AllCharsAreDigits(response, SecondFieldLen)) {
		result.resize(SecondFieldLen);
		std::copy(response, response + SecondFieldLen, result.begin());
		return success;
	}
	// skip 2nd field (probably MSISDN)
	response += SecondFieldLen;
	if (!TryToSkipDelimiters(response, end)) {
		return failure;
	}
	size_t ThirdFieldLen = strcspn(response, " \t\r\n");
	result.resize(ThirdFieldLen);
	std::copy(response, response + ThirdFieldLen, result.begin());
	return success;
}


ParseRes ResponseParser::ParseHLRResponse(const char* response, uint64_t imsi, const std::string& homeVlrGt, std::string& result)
{
	const char* end = response + strlen(response) - 1;
	if (!TryToSkipSubstring("HLR SUBSCRIBER DATA", response)) {
		return failure;
	}
	if (!TryToSkipSubstring("MSISDN           IMSI             STATE          AUTHD", response)) {
		return failure;
	}
	if (!TryToSkipDelimiters(response, end)) {
		return failure;
	}
	// try to find IMSI in next line of response and skip it. 
	// Next field after IMSI should be STATE
	char imsiStr[20];
	snprintf(imsiStr, sizeof(imsiStr), "%llu", imsi);
	if (!TryToSkipSubstring(imsiStr, response)) {
		return failure;
	}

	// One of states: CONNECTED or NOT CONNECTED should be in response.
	// Otherwise return failure.
	if (strstr(response, "NOT CONNECTED")) {
		result = "NOT CONNECTED";
		return success;
	}
	if (!strstr(response, "CONNECTED")) {
		return failure;
	}
	if (!TryToSkipSubstring("LOCATION DATA", response)) {
		return failure;
	}
	if (!TryToSkipSubstring("VLR ADDRESS       MSRN            MSC NUMBER          LMSID", response)) {
		return failure;
	}
	if (!TryToSkipDelimiters(response, end)) {
		return failure;
	}
	return ParseVlrAddr(response, homeVlrGt, result);
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


ParseRes ResponseParser::ParseVlrAddr(const char* response, const std::string& homeVlrGt, std::string& result)
{
	int vlrLength = strcspn(response, " \t\r\n");
	char vlr[30];
	strncpy_s(vlr, sizeof(vlr), response, vlrLength);
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
		result = vlr;
		return success;
	}
	
	const char* vlrAddr = strchr(vlr, '-');
	if (!vlrAddr) {
		return failure;
	}
	vlrAddr++;
	if (strcmp(vlrAddr, homeVlrGt.c_str())) {
		result = "ROAMING (" + std::string(vlrAddr) + ")";
	}
	else {
		result = vlrAddr;
	}
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
		*(end + 1) = '\0';
	}

	// replace \r and \n with spaces 
	for (char* p3 = start; p3 < end; p3++) {
		if (*p3 == '\r' || *p3 == '\n') {
			*p3 = ' ';
		}
	}
	result = start;
}