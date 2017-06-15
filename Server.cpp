#include <sstream>
#include <signal.h>
#include "Server.h"
#include "LogWriter.h"


extern LogWriter logWriter;
extern void CloseSocket(int socket);


Server::Server() :
    connectionPool(nullptr),
    shutdownInProgress(false)
{}


bool Server::Initialize(unsigned int port, ConnectionPool *cp, std::string& errDescription)
{
    connectionPool = cp;
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket < 0) {
        errDescription = "Unable to create server socket AF_INET, SOCK_STREAM.";
        return false;
	}
    int optval = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	struct sockaddr_in serverAddr;
	memset((char *) &serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);
	if (bind(listenSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) != 0) {
        errDescription = "Failed to call bind on server socket. Error " + std::to_string(WSAGetLastError());
        return false;
	}
	if (listen(listenSocket, MAX_CLIENT_CONNECTIONS) != 0) {
        errDescription = "Failed to call bind on server socket. Error " + std::to_string(WSAGetLastError());
        return false;
	}
    return true;
}


Server::~Server()
{
	CloseSocket(listenSocket);
	for (auto s : clientSockets) {
		if (s != 0) {
			CloseSocket(s);
		}
	}
}


void Server::Run()
{
	while (!shutdownInProgress) {
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100; //time-out = 100 ms
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(listenSocket, &readSet);
		int maxSocket = listenSocket;
		for (auto& s : clientSockets) {
			FD_SET(s, &readSet);
			if (s > maxSocket) {
				maxSocket = s;
			}
		}
		const int SELECT_TIMEOUT = 0;
		int socketCount;
		if ((socketCount = select(maxSocket + 1, &readSet, NULL, NULL, &tv)) != SELECT_TIMEOUT) {
			if (socketCount == SOCKET_ERROR) {
				logWriter << "socket select function returned error: " + std::to_string(WSAGetLastError());
                continue;
			}
			if (FD_ISSET(listenSocket, &readSet)) {
				ProcessIncomingConnection();
			}
            auto it = clientSockets.begin();
            while (it != clientSockets.end()) {
                if (FD_ISSET(*it, &readSet)) {
					char receiveBuffer[65000];
					int recvBytes = recv(*it, receiveBuffer, sizeof(receiveBuffer), 0);
                    if (recvBytes > 0) {
                        if (!ProcessIncomingData(*it, receiveBuffer, recvBytes)) {
                            logWriter << "Error receiving data on connection #" + std::to_string(*it) + " (error code "
                                + std::to_string(WSAGetLastError()) + "). Closing connection...";
                        }
                        it++;
                    }
                    else {
						if (recvBytes == 0) {
							logWriter << "Client " + GetClientIPAddr(*it) + " disconnected.";
						}
						else {
							logWriter << "Error receiving data from " + GetClientIPAddr(*it) + " (error code "
								+ std::to_string(WSAGetLastError()) + "). Closing connection...";
						}
						CloseSocket(*it);
                        clientAddrs.erase(*it);
                        clientSockets.erase(it++);
					}
				}
                else {
                    it++;
                }
			}
		}
	}
}


bool Server::ProcessIncomingConnection()
{
    #ifdef WIN32
      int clilen;
    #else
      socklen_t clilen;
    #endif
    struct sockaddr_in newClientAddr;
    clilen = sizeof(newClientAddr);
    int newSocket = accept(listenSocket, (struct sockaddr*) &newClientAddr, &clilen);
    if (newSocket < 0) {
       logWriter << "Failed to call accept on socket: " + std::to_string(WSAGetLastError());
       return false;
    }

	logWriter << "Incoming connection from " + IPAddr2Text(newClientAddr.sin_addr);
    if (shutdownInProgress) {
        logWriter << "Shutdown in progress. Rejecting connection...";
        CloseSocket(newSocket);
        return false;
    }

    clientSockets.push_back(newSocket);
	clientAddrs.insert(std::make_pair(newSocket, newClientAddr.sin_addr));
    return true;
}


bool Server::ProcessIncomingData(int socket, const char* buffer, int bufferSize)
{
	int bytesProcessed = 0;
    while(bytesProcessed < bufferSize) {
      int requestLen = ProcessNextRequestFromBuffer(socket, buffer + bytesProcessed, bufferSize - bytesProcessed);
	  if (requestLen >= 0) {
		  bytesProcessed += requestLen;
	  }
	  else {
		  break;
	  }
    }
    return true;
}

/* Function returns length of next successfully processed request from buffer 
	or PARSE_ERROR in case of failure.
*/
int Server::ProcessNextRequestFromBuffer(int socket, const char* buffer, int maxLen)
{
	std::multimap<__uint16_t, SPSReqAttrParsed> requestAttrs;
	CPSPacket pspRequest;
	uint32_t requestNum;
    uint16_t requestType;
    uint16_t packetLen;
    const int VALIDATE_PACKET = 1;
    
    int parseRes = pspRequest.Parse((SPSRequest *)buffer, maxLen,
                      requestNum, requestType, packetLen, requestAttrs, VALIDATE_PACKET);
	if (parseRes == PARSE_ERROR) {
		logWriter.Write("Unable to parse incoming data.", mainThreadIndex, error);
		// TODO: send response ?
		return PARSE_ERROR;
	}
	std::string errorDescr;
	if(requestType != COMMAND_REQ) {
		errorDescr = "Unsupported request type " + std::to_string(requestType);
        logWriter.Write(errorDescr, mainThreadIndex, error);
		SendNotAcceptedResponse(socket, requestNum, errorDescr);
        return packetLen;
    }
    logWriter.Write("Request #" + std::to_string(requestNum) + " received from " + GetClientIPAddr(socket),
                        mainThreadIndex, notice);
	ClientRequest clientRequest(socket);
	if (!clientRequest.ValidateAndSetRequestParams(requestNum, requestAttrs, clientRequest, errorDescr)) {
        logWriter.Write("Request #" + std::to_string(requestNum) + " rejected due to: " + errorDescr,
                        mainThreadIndex, error);
		SendNotAcceptedResponse(socket, requestNum, errorDescr);
		return packetLen;
	}
	
	unsigned int connIndex;
    if (!connectionPool->TryAcquire(connIndex)) {
		errorDescr = "Unable to acqure connection to HLR for request execution.";
        logWriter.Write("Request #" + std::to_string(requestNum) + " rejected due to: " + errorDescr,
                        mainThreadIndex, error);
		SendNotAcceptedResponse(socket, requestNum, errorDescr);
		return packetLen;
	}
	logWriter.Write("Acquired connection #" + std::to_string(connIndex), mainThreadIndex, debug);
    clientRequest.resultCode = connectionPool->ExecRequest(connIndex, clientRequest);
	logWriter << clientRequest.DumpResults();
	if (!clientRequest.SendRequestResultToClient(errorDescr)) {
		logWriter.Write("SendRequestResultToClient error: " + errorDescr, mainThreadIndex, error);
	}
	return packetLen;
}


bool Server::SendNotAcceptedResponse(int socket, uint32_t requestNum,  std::string errDescr)
{
	CPSPacket pspResponse;
	char buffer[2014];
    if(pspResponse.Init(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer), requestNum, COMMAND_RSP) != 0) {
        logWriter.Write("SendNotAcceptedResponse error: initializing response buffer failed", mainThreadIndex, error);
        return false;
    }
	int8_t errorCode = BAD_CLIENT_REQUEST;
	unsigned long len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer), 
			PS_RESULT, &errorCode, sizeof(errorCode));
	len = pspResponse.AddAttr(reinterpret_cast<SPSRequest*>(buffer), sizeof(buffer), 
			PS_DESCR, errDescr.data(), errDescr.size());
	if(send(socket, buffer, len, 0) <= 0) {
        logWriter.Write("SendNotAcceptedResponse error: socket error " + std::to_string(WSAGetLastError()), mainThreadIndex, error);
        return false;
    }
	return true;
}



std::string Server::GetClientIPAddr(int socket)
{
	auto it = clientAddrs.find(socket);
	if (it != clientAddrs.end()) {
		return IPAddr2Text(it->second);
	}
	else {
		return "<UNKNOWN CLIENT>";
	}
}


std::string Server::IPAddr2Text(const in_addr& inAddr)
{
	char buffer[64];
#ifdef WIN32
    _snprintf_s(buffer, sizeof(buffer) - 1, "%d.%d.%d.%d", inAddr.S_un.S_un_b.s_b1, inAddr.S_un.S_un_b.s_b2, 
		inAddr.S_un.S_un_b.s_b3, inAddr.S_un.S_un_b.s_b4);
#else
    inet_ntop(AF_INET, (const void*) &inAddr, buffer, sizeof(buffer) - 1);
#endif
	return std::string(buffer);
}

void Server::Stop()
{
    shutdownInProgress = true;
}



