#include <sstream>
#include "ConnectionPool.h"


extern LogWriter logWriter;

ConnectionPool::ConnectionPool() :
    initialized(false),
    stopFlag(false),
    incomingRequests(queueSize)
{
}


ConnectionPool::~ConnectionPool()
{
    stopFlag = true;
    conditionVar.notify_all();
    for(auto& thr : workerThreads) {
        if (thr.joinable()) {
            thr.join();
        }
    }
    for(auto& conn : hlrConnectPool) {
        delete conn;
    }
}


bool ConnectionPool::Initialize(const Config& config, std::string& errDescription)
{
    if (initialized) {
        errDescription = "Connection pool is already initialized";
        return false;
    }

    for (unsigned int i = 0; i < config.connectionCount; ++i) {
        HLRConnector* conn = new HLRConnector(i, config);
        hlrConnectPool.push_back(conn);
        if (!conn->Initialize()) {
            errDescription = conn->GetErrDescription();
            return false;
        }
        workerThreads.push_back(std::thread(&ConnectionPool::WorkerThread, this, i, conn));
    }
    logWriter << "Connection pool initialized successfully";
    initialized = true;
    return true;
}


void ConnectionPool::PushRequest(ClientRequest *clientRequest)
{
    incomingRequests.push(clientRequest);
    conditionVar.notify_one();
}


void ConnectionPool::WorkerThread(unsigned int index, HLRConnector* hlrConnect)
{
    while (!stopFlag) {
        std::unique_lock<std::mutex> ul(lock);
        conditionVar.wait(ul);
        ul.unlock();
        ClientRequest* request;

        while (incomingRequests.pop(request)) {
            double requestAgeSec = duration<double>(steady_clock::now() - request->accepted).count();
            if (requestAgeSec <= maxRequestAgeSec) {
                ProcessRequest(index, request, hlrConnect);
                std::stringstream ss;
                ss << "Request #" << request->requestNum << " processed in "
                   << round(duration<double>(steady_clock::now() - request->accepted).count() * 1000)  << " ms. ";
                ss << request->DumpResults();
                logWriter.Write(ss.str(), index);

                std::string errorDescr;
                if (!request->SendRequestResultToClient(errorDescr)) {
                    logWriter.Write("SendRequestResultToClient failed: " + errorDescr, index, error);
                }
                delete request;
            }
            else {
                std::stringstream ss;
                ss << "Request #" << request->requestNum << " discarded due to max age exceeding ("
                   << round(requestAgeSec * 1000) << " ms)";
                logWriter << ss.str();
            }
        }
    }
}


void ConnectionPool::ProcessRequest(unsigned int index, ClientRequest* request, HLRConnector *hlrConnect)
{
    RequestedDevice requestedDevice;
    std::string deviceCommand;
    switch(request->requestType) {
    case stateQuery:
        requestedDevice = VLR;
        deviceCommand = "MGSSP: IMSI=" + std::to_string(request->subscriberID) + ";";
        break;
    case resetRequest:
        requestedDevice = HLR;
        deviceCommand = "HGSLR: IMSI=" + std::to_string(request->subscriberID) + ";";
        break;
    case activateRequest:
        requestedDevice = HLR;
        deviceCommand = "HGSDC: MSISDN=" + std::to_string(request->subscriberID) + ",SUD=TS11-1;";
        break;
    case deactivateRequest:
        requestedDevice = HLR;
        deviceCommand = "HGSDC: MSISDN=" + std::to_string(request->subscriberID) + ",SUD=TS11-0;";
        break;
    default:
        request->resultCode = CMD_UNKNOWN;
        request->resultDescr = "ConnectionPool: unknown request "
                + std::to_string(request->requestType);
        return;
    }

    std::string response;
    logWriter.Write("Processing command: " + deviceCommand, index);
    if(!hlrConnect->ProcessCommand(deviceCommand, response)) {
        request->resultCode = CMD_NOTEXECUTED;
        request->resultDescr = "Failed to process VLR/HLR command: "+ hlrConnect->GetErrDescription();
        return;
    }
    if (request->requestType == stateQuery) {
        ParseRes res = ResponseParser::Parse(requestedDevice, response.c_str(), *request);
        if (res == success) {
            request->resultCode = OPERATION_SUCCESS;
        }
        else if (res == infoNotComplete) {
            requestedDevice = HLR;
            deviceCommand = "HGSDP: IMSI=" + std::to_string(request->subscriberID) + ",LOC;";
            if(!hlrConnect->ProcessCommand(deviceCommand, response)) {
                request->resultCode = CMD_NOTEXECUTED;
                request->resultDescr = "Failed to process HLR command: "+ hlrConnect->GetErrDescription();
                return;
            }
            res = ResponseParser::Parse(requestedDevice, response.c_str(), *request);
            if (res == success) {
                request->resultCode = OPERATION_SUCCESS;
            }
            else {
                request->resultCode = BAD_DEVICE_RESPONSE;
                request->resultDescr = "Unable to parse device response";
            }

        }
        else if (res == failure) {
            request->resultCode = BAD_DEVICE_RESPONSE;
            request->resultDescr = "Unable to parse device response";
        }
    }
    else {
        request->resultCode = OPERATION_SUCCESS;
    }
}

