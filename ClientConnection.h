#pragma once
#include <memory>
#include "Common.h"

class ClientConnection
{
public:
	ClientConnection(int socket);
	~ClientConnection();
private:
	int socket;
	in_addr inAddr;
};

typedef std::shared_ptr<ClientConnection> ClientConnectionPtr;