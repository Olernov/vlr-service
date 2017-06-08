#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Common.h"
#include "ResponseParser.h"
#include "Config.h"


class ConnectionPool
{
public:
	ConnectionPool(const Config& config);
	~ConnectionPool();
	bool Initialize(const Config& config, std::string& errDescription);
	bool TryAcquire(unsigned int& index);
	int8_t ExecRequest(unsigned int index, ClientRequest& clientRequest);
	
private:
	static const int receiveBufferSize = 65000;
	static const int sendBufferSize = 1024;
	int m_sockets[MAX_THREADS];
	std::vector<std::thread> m_threads;
	bool m_connected[MAX_THREADS];
	std::atomic_bool m_busy[MAX_THREADS];
	std::atomic_int m_lastUsed;
	bool m_finished[MAX_THREADS];
	std::condition_variable m_condVars[MAX_THREADS];
	std::mutex m_mutexes[MAX_THREADS];
	ClientRequest* m_requests[MAX_THREADS];
	
	//uint64_t m_imsis[MAX_THREADS];
	int m_resultCodes[MAX_THREADS];
	std::string m_results[MAX_THREADS];
	const Config& m_config;
	bool m_stopFlag;

	enum {
		TELNET_SE = 240,	/* End of subnegotiation parameters. */
		TELNET_NOP = 241,	/* No operation. */
		TELNET_DM = 242,	/* (Data Mark) The data stream portion of a
			* Synch. This should always be accompanied
			* by a TCP Urgent notification. */
		TELNET_BRK = 243,	/* (Break) NVT character BRK. */
		TELNET_IP = 244,	/* (Interrupt Process) The function IP. */
		TELNET_AO = 245,	/* (Abort output) The function AO. */
		TELNET_AYT = 246,	/* (Are You There) The function AYT. */
		TELNET_EC = 247,	/* (Erase character) The function EC. */
		TELNET_EL = 248,	/* (Erase Line) The function EL. */
		TELNET_GA = 249,	/* (Go ahead) The GA signal. */
		TELNET_SB = 250,	/* Indicates that what follows is
			* subnegotiation of the indicated option. */
		TELNET_WILL = 251,	/* Indicates the desire to begin performing,
			* or confirmation that you are now performing,
			* the indicated option. */
		TELNET_WONT = 252,	/* Indicates the refusal to perform, or to
			* continue performing, the indicated option. */
		TELNET_DO = 253,	/* Indicates the request that the other party
			* perform, or confirmation that you are
			* expecting the other party to perform, the
			* indicated option. */
		TELNET_DONT = 254,	/* Indicates the demand that the other party
			* stop performing, or confirmation that you
			* are no longer expecting the other party
			* to perform, the indicated option. */
		TELNET_IAC = 255	/* Data Byte 255. */
	};
	
	void WorkerThread(unsigned int index);
	bool ConnectSocket(unsigned int index, std::string& errDescription);
	bool LoginToHLR(unsigned int index, std::string& errDescription);
	bool Reconnect(unsigned int index, std::string& errDescription);
	int RestoreConnectionIfNeeded(unsigned int index, std::string& errDescription);
	void TelnetParse(unsigned char* recvbuf, int* bytesRecv, int socketFD);
	void FinishWithNetworkError(std::string logMessage, unsigned int index);
	int SendCommandToDevice(unsigned int index, std::string hlrCommand, std::string& errDescription);
	int ProcessDeviceResponse(unsigned int index, RequestedDevice requestedDevice, RequestType requestType,
		std::string deviceCommand, std::string& errDescription);
	std::string GetWinsockError();
};

