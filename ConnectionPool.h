#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Config.h"
#include "ClientRequest.h"
#include "HLRConnector.h"

class ConnectionPool
{
public:
    ConnectionPool();
    ~ConnectionPool();
    bool Initialize(const Config& config, std::string& errDescription);
    void PushRequest(ClientRequest *clientRequest);
    ClientRequest* PopProcessedRequest();
private:
    static const int queueSize = 1024;
    static const int maxRequestAgeSec = 10;
    std::string connectString;
    bool initialized;
    bool stopFlag;
    std::vector<HLRConnector*> hlrConnectPool;
    std::vector<std::thread> workerThreads;
    std::condition_variable conditionVar;
    std::mutex lock;
    boost::lockfree::queue<ClientRequest*> incomingRequests;

    void WorkerThread(unsigned int index, HLRConnector *hlrConnect);
    void ProcessRequest(unsigned int index, ClientRequest* request, HLRConnector* dbConnect);
};

