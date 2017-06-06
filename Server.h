#pragma once
#include <list>
#include <map>
#include "PS_Common.h"
#include "PSPacket.h"
#include "ConnectionPool.h"
#include "ClientConnection.h"



class Server
{
public:
	Server(unsigned int port, ConnectionPool& cp);
	~Server();
	void Run();
private:
	static const int MAX_CLIENT_CONNECTIONS = 10;
	static const int PARSE_ERROR = -1;

	int listenSocket;
	//std::list<ClientConnectionPtr> clientConnections;
	std::list<int> clientSockets;
	std::map<int, in_addr> clientAddrs;
	ConnectionPool& connectionPool;
	bool shutdownInProgress;

	bool ProcessIncomingConnection();
	bool ProcessIncomingData(int socket, const char* buffer, int bufferSize);
	int ProcessNextRequestFromBuffer(int socket, const char* buffer, int maxLen);
	bool ValidateAndSetRequestParams(uint32_t requestNum, const std::multimap<__uint16_t, SPSReqAttrParsed>& requestAttrs,
		ClientRequest& clientRequest, std::string& errorDescr);
	bool SendRequestResultToClient(const ClientRequest& clientRequest);
	bool SendNotAcceptedResponse(int socket, uint32_t requestNum, std::string errDescr);
	std::string GetClientIPAddr(int socket);
	std::string IPAddr2Text(const in_addr& pinAddr);
};

