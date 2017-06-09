#ifdef _WIN32
	#include <Winsock2.h>
#endif
#include <sstream>
#include "ClientRequest.h"
#include "Common.h"


ClientRequest::ClientRequest(int socket) : 
	socket(socket), 
	vlrAddress(0) 
{}


bool ClientRequest::ValidateAndSetRequestParams(uint32_t reqNum, const std::multimap<__uint16_t, SPSReqAttrParsed>& requestAttrs, 
	ClientRequest& clientRequest, std::string& errorDescr)
{
	requestNum = reqNum;
	auto iter = requestAttrs.find(VLR_GATEWAY_COMMAND);
	if (iter == requestAttrs.end()) {
		errorDescr  = "VLR command type is missing in request";
		return false;
	}
	if (iter->second.m_usDataLen != 1) {
		errorDescr  = "VLR command type param has incorrect size " + std::to_string(iter->second.m_usDataLen) +
			". Its size must be 1 byte.";
		return false;
	}
	uint8_t rt = *static_cast<uint8_t*>(iter->second.m_pvData);
	requestType = static_cast<RequestType>(rt);
	if (requestType != stateQuery && requestType != resetRequest) {
		errorDescr = "Request type " + std::to_string(requestType) + " is not implemented";
		return false;
	}

	iter = requestAttrs.find(VLR_GATEWAY_IMSI);
	if (iter == requestAttrs.end()) {
		errorDescr  = "IMSI is missing in request";
		return false;
	}
	if (iter->second.m_usDataLen != 8) {
		errorDescr  = "IMSI param has incorrect size " + std::to_string(iter->second.m_usDataLen) +
			". Its size must be 8 bytes.";
		return false;
	}
	clientRequest.imsi = ntohll(*static_cast<uint64_t*>(iter->second.m_pvData));
	return true;
}


bool ClientRequest::SendRequestResultToClient(std::string& errorDescr)
{
	CPSPacket pspResponse;
	char buffer[2014];
    if(pspResponse.Init(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer), requestNum, COMMAND_RSP) != 0) {
		errorDescr = "PSPacket init failed";
        return false;
    }
	unsigned long len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer), 
			PS_RESULT, &resultCode, sizeof(resultCode));
	if (resultCode == OPERATION_SUCCESS) {
		if (requestType == stateQuery) {
			uint8_t state = subscriberState;
			len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer),
				VLR_GATEWAY_STATE, &state, sizeof(state));
			if (subscriberState == connected) {
				uint8_t online = subscriberOnline;
				len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer),
					VLR_GATEWAY_ONLINE_STATUS, &online, sizeof(online));
			}
			else if (subscriberState == roaming) {
				uint64_t vlrAddr = htonll(vlrAddress);
				len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer),
					VLR_GATEWAY_VLR_ADDRESS, &vlrAddr, sizeof(vlrAddr));
			}
		}
	}
	else {
		len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer),
			PS_DESCR, resultDescr.data(), resultDescr.size());
	}

	if(send(socket, buffer, len, 0) <= 0) {
        errorDescr = "socket error " + std::to_string(WSAGetLastError());
        return false;
    }
	return true;
}


std::string ClientRequest::DumpResults()
{
	std::stringstream ss;
	ss << "Result code: " << std::to_string(resultCode);
	if (resultCode == OPERATION_SUCCESS) {
		if (requestType == stateQuery) {
			ss << std::endl << "SubscriberState: " << subscriberState;
			if (subscriberState == connected) {
				ss << std::endl << "SubscriberOnline: " << subscriberOnline;
			}
			if (subscriberState == roaming) {
				ss << std::endl << "VLR address: " << vlrAddress;
			}
		}
	}
	else {
		ss << " (" << resultDescr << ")";
	}
	return ss.str();
}