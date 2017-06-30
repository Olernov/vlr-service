#pragma once
#include <list>
#include <map>
#include "ps_common.h"
#include "pspacket.h"
#include "ConnectionPool.h"
#include "ClientConnection.h"
#include "ClientRequest.h"


class Server
{
public:
    Server();
    bool Initialize(unsigned int port, ConnectionPool* cp, std::string &errDescription);
	~Server();
	void Run();
    void Stop();

private:
	static const int MAX_CLIENT_CONNECTIONS = 10;
	static const int PARSE_ERROR = -1;

	int listenSocket;
    std::list<int> clientSockets;
	std::map<int, in_addr> clientAddrs;
    ConnectionPool* connectionPool;
	bool shutdownInProgress;

	bool ProcessIncomingConnection();
    void ProcessIncomingData(int socket, const char* buffer, int bufferSize);
	int ProcessNextRequestFromBuffer(int socket, const char* buffer, int maxLen);
	bool SendNotAcceptedResponse(int socket, uint32_t requestNum, std::string errDescr);
	std::string GetClientIPAddr(int socket);
	std::string IPAddr2Text(const in_addr& pinAddr);

};

