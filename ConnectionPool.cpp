#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <chrono>
#include <thread>
#include "Config.h"
#include "ConnectionPool.h"
#include "LogWriter.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#ifndef _WIN32
    char* _strupr_s(char* s, size_t size)
    {
      const char* end = s + size;
      char* p = s;
      while ((*p != STR_TERMINATOR) && (p < end)) {
          *p = toupper(*p);
          p++;
      }
      return s;
    }

//    int strncat_s(char *dest, size_t destsz, const char *src, size_t count)
//    {
//        strncat(dest, src, count);
//        return 0;
//    }

//    int strncpy_s(char* dest, size_t destsz,
//                      const char* src, size_t count)
//    {
//        strncpy(dest, src,count);
//        return 0;
//    }

#endif


extern LogWriter logWriter;
extern void CloseSocket(int socket);

ConnectionPool::ConnectionPool(const Config& config) :
	m_config(config),
	m_stopFlag(false)
{
	m_lastUsed.store(0);
}


ConnectionPool::~ConnectionPool()
{
	for(auto& socket : m_sockets) {
		CloseSocket(socket);
	}
	m_stopFlag = true;
	for (int i = 0; i < m_config.connectionCount; ++i) {
		m_condVars[i].notify_one();
	}
	for(auto& thr : m_threads) {
		if (thr.joinable())
			thr.join();
	}

}


bool ConnectionPool::Initialize(const Config& config, std::string& errDescription)
{
#ifdef WIN32
	WSADATA wsaData;
	if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
        errDescription = "Error initializing Winsock: " + std::to_string(WSAGetLastError());
		return false;
	}
#endif

	for (unsigned int index = 0; index < config.connectionCount; ++index) {
		m_sockets[index] = INVALID_SOCKET;
		if (!ConnectSocket(index, errDescription)) {
			return false;
		}
		m_connected[index] = true;
		logWriter.Write("Connected to HLR successfully.", index+1);
		
#ifndef DONT_LOGIN_TO_HLR		
			if (!LoginToHLR(index, errDescription)) {
				return false;
			}
#endif
		m_busy[index] = false;
		m_finished[index] = false;		
	}
	
	for (unsigned int i = 0; i < config.connectionCount; ++i) {
		m_threads.push_back(std::thread(&ConnectionPool::WorkerThread, this, i));
	}
	return true;
}


bool ConnectionPool::ConnectSocket(unsigned int index, std::string& errDescription)
{
    struct sockaddr_in addr;
    m_sockets[index] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sockets[index] == INVALID_SOCKET) {
		errDescription = "Error creating socket. " + GetWinsockError();
		return false;
	}
	memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(m_config.vlrAddr.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
		errDescription = "Error parsing host address: " + m_config.vlrAddr + GetWinsockError();
		return false;
	}
    addr.sin_port = htons(m_config.vlrPort);
    if(connect(m_sockets[index], reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		errDescription = "Unable to connect to host " + m_config.vlrAddr + GetWinsockError();
		return false;
    }
	u_long iMode=1;
    if(ioctlsocket(m_sockets[index], FIONBIO, &iMode) != 0) {
		errDescription = "Error setting socket in non-blocking mode. " + GetWinsockError();
		return false;
	}
	return true;
}


bool ConnectionPool::LoginToHLR(unsigned int index, std::string& errDescription)
{
	fd_set read_set, error_set;
    struct timeval tv;
	int bytesRecv = 0;
	char recvbuf[receiveBufferSize];
	char sendbuf[sendBufferSize]; 
	int nAttemptCounter = 0;

	while(nAttemptCounter < 10) {
		logWriter.Write("Login attempt " + std::to_string(nAttemptCounter+1), index+1, debug);
		while(true) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			FD_ZERO( &read_set );
			FD_SET( m_sockets[index], &read_set );
			if (select( m_sockets[index] + 1, &read_set, NULL, &error_set, &tv ) != 0 ) {
				// check for message
				if (FD_ISSET( m_sockets[index], &read_set))  {
					// receive some data from server
					recvbuf[0] = STR_TERMINATOR;
					if ((bytesRecv = recv( m_sockets[index], recvbuf, sizeof(recvbuf), 0 ) ) == SOCKET_ERROR)  {
						errDescription = "LoginToHLR: Error receiving data from host" + GetWinsockError();
						return false;
					}
					else {
						TelnetParse((unsigned char*) recvbuf, &bytesRecv, m_sockets[index]);
						if (bytesRecv>0) {
							recvbuf[bytesRecv] = STR_TERMINATOR;
							logWriter.Write("LoginToHLR: HLR response: " + std::string(recvbuf), index+1, debug);
                            _strupr_s(recvbuf, receiveBufferSize);
							if(strstr(recvbuf, "LOGIN:")) {
								// server asks for login
								logWriter.Write("Sending username: " + m_config.username, debug);
								sprintf_s((char*) sendbuf, sendBufferSize, "%s\r\n", m_config.username.c_str());
								if(send( m_sockets[index], sendbuf, strlen(sendbuf), 0 ) == SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf,"PASSWORD:")) {
								// server asks for password
								logWriter.Write("Sending password: " + m_config.password, index+1, debug);
								sprintf_s((char*) sendbuf, sendBufferSize, "%s\r\n", m_config.password.c_str());
								if(send(m_sockets[index], sendbuf, strlen(sendbuf), 0) == SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf,"DOMAIN:")) {
								// server asks for domain
								logWriter.Write("Sending domain: " + m_config.domain, index+1, debug);
								sprintf_s((char*)sendbuf, sendBufferSize, "%s\r\n", m_config.domain.c_str());
								if(send( m_sockets[index], sendbuf,strlen(sendbuf), 0 )==SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf,"TERMINAL TYPE?")) {
								// server asks for terminal type
								logWriter.Write(std::string("Sending terminal type: ") + TERMINAL_TYPE, index+1, debug);
								sprintf_s((char*)sendbuf, sendBufferSize, "%s\r\n", TERMINAL_TYPE);
								if(send( m_sockets[index], sendbuf, strlen(sendbuf), 0 )==SOCKET_ERROR) {
									errDescription = "Error sending data on socket" + GetWinsockError();
									return false;
								}
								continue;
							}
							if(strstr(recvbuf, HLR_PROMPT)) {
								return true;
							}
							if(strstr(recvbuf, "AUTHORIZATION FAILURE")) {
								errDescription = "LoginToHLR: authorization failure.";
								return false;
							}
							if(strstr(recvbuf, "SUCCESS")) {
								return true;
							}
							if(char* p=strstr(recvbuf,"ERR")) {
								// cut info from response
								size_t pos=strcspn(p,";\r\n");
								*(p + pos) = STR_TERMINATOR;
								errDescription =  "Unable to log in HLR: " + std::string(p);
								return false;
							}
						}
					}
				}
			}
			else {
				logWriter.Write("LoginToHLR: select time-out", index+1, debug);
				nAttemptCounter++;
				break;
			}
		}

	}
	errDescription = "Unable to login to Ericsson HLR.";
	return false;
}


bool ConnectionPool::Reconnect(unsigned int index, std::string& errDescription)
{
	logWriter.Write("Trying to reconnect ...", index);
	CloseSocket(m_sockets[index]);
	m_connected[index] = false;
	if(ConnectSocket(index, errDescription)) {
		if(LoginToHLR(index, errDescription)) {
			logWriter.Write("Reconnected and logged in successfully", index);
			m_connected[index] = true;
			return true;
		}
	}
	
	logWriter.Write("Unable to reconnect.", index);
	return false;
}


// This code is taken from NetCat project http://netcat.sourceforge.net/
void ConnectionPool::TelnetParse(unsigned char* recvbuf,int* bytesRecv,int socketFD)
{
	/*static*/ unsigned char getrq[4];
	unsigned char putrq[4], *buf=recvbuf;
	int eat_chars=0;
	int l = 0;
	/* loop all chars of the string */
	for(int i=0; i<*bytesRecv; i++)
	{
		 if (recvbuf[i]==0) {
			// иногда в ответах HLR проскакивают нули, которые воспринимаются как концы строк. Заменим их на пробел
			recvbuf[i]=' ';
			continue;
		}

		/* if we found IAC char OR we are fetching a IAC code string process it */
		if ((recvbuf[i] != TELNET_IAC) && (l == 0))
			continue;
		/* this is surely a char that will be eaten */
		eat_chars++;
		/* copy the char in the IAC-code-building buffer */
		getrq[l++] = recvbuf[i];
		/* if this is the first char (IAC!) go straight to the next one */
		if (l == 1)
			continue;
		/* identify the IAC code. The effect is resolved here. If the char needs
   further data the subsection just needs to leave the index 'l' set. */
	switch (getrq[1]) {
		case TELNET_SE:
		case TELNET_NOP:
		  goto do_eat_chars;
		case TELNET_DM:
		case TELNET_BRK:
		case TELNET_IP:
		case TELNET_AO:
		case TELNET_AYT:
		case TELNET_EC:
		case TELNET_EL:
		case TELNET_GA:
		case TELNET_SB:
		  goto do_eat_chars;
		case TELNET_WILL:
		case TELNET_WONT:
		  if (l < 3) /* need more data */
			continue;

		  /* refuse this option */
		  putrq[0] = 0xFF;
		  putrq[1] = TELNET_DONT;
		  putrq[2] = getrq[2];
		  /* FIXME: the rfc seems not clean about what to do if the sending queue
			 is not empty.  Since it's the simplest solution, just override the
			 queue for now, but this must change in future. */
		  //write(ncsock->fd, putrq, 3);		
		  send( socketFD, (char*)putrq,3, 0 );
/* FIXME: handle failures */
		  goto do_eat_chars;
		case TELNET_DO:
		case TELNET_DONT:
		  if (l < 3) /* need more data */
			continue;

		  /* refuse this option */
		  putrq[0] = 0xFF;
		  putrq[1] = TELNET_WONT;
		  putrq[2] = getrq[2];
		  //write(ncsock->fd, putrq, 3);
		  send( socketFD, (char*)putrq,3, 0 );
		  goto do_eat_chars;
		case TELNET_IAC:
		  /* insert a byte 255 in the buffer.  Note that we don't know in which
			 position we are, but there must be at least 1 eaten char where we
			 can park our data byte.  This effect is senseless if using the old
			 telnet codes parsing policy. */
		  buf[i - --eat_chars] = 0xFF;
		  goto do_eat_chars;
		
		default:
		  /* FIXME: how to handle the unknown code? */
		  break;
		}
	continue;

do_eat_chars:
	/* ... */
	l = 0;
	if (eat_chars > 0) {
	  unsigned char *from, *to;

	  /* move the index to the overlapper character */
	  i++;

	  /* if this is the end of the string, memmove() does not care of a null
		 size, it simply does nothing. */
	  from = &buf[i];
	  to = &buf[i - eat_chars];
	  memmove(to, from, *bytesRecv - i);

	  /* fix the index.  since the loop will auto-increment the index we need
		 to put it one char before.  this means that it can become negative
		 but it isn't a big problem since it is signed. */
	  i -= eat_chars + 1;
	  *bytesRecv -= eat_chars;
	  eat_chars = 0;
	  }
	}
}

void ConnectionPool::FinishWithNetworkError(std::string logMessage, unsigned int index)
{
	logWriter.Write("ExecuteCommand: " + logMessage + GetWinsockError(), index);
	m_resultCodes[index] = NETWORK_ERROR;
	m_results[index] = "Network error sending message to HLR";
	m_finished[index] = true;
	m_condVars[index].notify_one();
}


int ConnectionPool::SendCommandToDevice(unsigned int index, std::string deviceCommand, std::string& errDescription)
{
	int restoreRes = RestoreConnectionIfNeeded(index, errDescription);
	if (restoreRes != OPERATION_SUCCESS) {
		return restoreRes;
	}
	char sendBuf[sendBufferSize];
    sprintf_s(sendBuf, sendBufferSize, "%s\r\n", deviceCommand.c_str());
	if (send(m_sockets[index], (char*)sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
		errDescription = "Socket error when sending command" + GetWinsockError();
		return NETWORK_ERROR;
	}
	return OPERATION_SUCCESS;
}


int ConnectionPool::ProcessDeviceResponse(unsigned int index, RequestedDevice requestedDevice, RequestType requestType,
	std::string deviceCommand, std::string& errDescription)
{
	char recvBuf[receiveBufferSize];
	char hlrResponse[receiveBufferSize];
	hlrResponse[0] = STR_TERMINATOR;
	fd_set read_set;
	struct timeval tv;
	while (!m_finished[index]) {
		tv.tv_sec = SOCKET_TIMEOUT_SEC;
		tv.tv_usec = 0;
		FD_ZERO(&read_set);
		FD_SET(m_sockets[index], &read_set);
		if (select(m_sockets[index] + 1, &read_set, NULL, NULL, &tv) != 0) {
			// check for message
			if (FD_ISSET(m_sockets[index], &read_set)) {
				// receive some data from server
				int bytesRecv = recv(m_sockets[index], recvBuf, receiveBufferSize, 0);
				if (bytesRecv == SOCKET_ERROR)    {
					errDescription = "Socket error when receiving data" + GetWinsockError();
					return NETWORK_ERROR;
				}
				else {
					TelnetParse((unsigned char*)recvBuf, &bytesRecv, m_sockets[index]);
					if (bytesRecv > 0) {
						recvBuf[bytesRecv] = STR_TERMINATOR;
						logWriter.Write(std::string("HLR response: ") + recvBuf, index, debug);
						_strupr_s(recvBuf, receiveBufferSize);
                        strncat(hlrResponse, recvBuf, bytesRecv + 1);
						if (requestType == stateQuery && strstr(hlrResponse, "END")) {							
							ParseRes res = ResponseParser::Parse(requestedDevice, hlrResponse, *m_requests[index]);
							if (res == success) {
								return OPERATION_SUCCESS;
							}
							else if (res == infoNotComplete) {
								return INFO_NOT_COMPLETE;
							}
							else {
								errDescription = m_requests[index]->resultDescr;
								return BAD_DEVICE_RESPONSE;
							}
						}
						else if (requestType == resetRequest && strstr(hlrResponse, "EXECUTED"))  {
							return OPERATION_SUCCESS;
						}
						if (char* p = strstr(hlrResponse, "NOT ACCEPTED")) {
							ResponseParser::StripHLRResponse(p + strlen("NOT ACCEPTED"), errDescription);
							return CMD_NOTEXECUTED;
						}
						if (strstr(hlrResponse, deviceCommand.c_str())
								&& !strcmp(hlrResponse + strlen(hlrResponse) - 2, "HLR_PROMPT")) {
							// if HLR answers with echo and prompt then send ';'
							logWriter.Write("Command echo received. Sending CRLF ...", index, debug);
							const char* crlf = "\r\n";
							if (send(m_sockets[index], crlf, strlen(crlf), 0) == SOCKET_ERROR) {
								errDescription = "Socket error when sending data" + GetWinsockError();
								return NETWORK_ERROR;
							}
							hlrResponse[0] = STR_TERMINATOR; // start composing new HLR response
						}
					}
					else {
						logWriter.Write("No bytes to read", index, debug);
						if (strlen(hlrResponse)) {
							errDescription = std::string("Unable to parse VLR/HLR response:\n") + hlrResponse;
							return BAD_DEVICE_RESPONSE;
						}
						else {
							errDescription = "No response received from VLR/HLR.";
							return BAD_DEVICE_RESPONSE;
						}
					}
				}
			}
		}
		else {
			logWriter.Write("ExecuteCommand: socket time-out", index, debug);
			if (strlen(hlrResponse) > 0) {
				errDescription = std::string("Unable to parse HLR response:\n") + hlrResponse;
				return CMD_UNKNOWN;
			}
			else {
				errDescription = "No response received from HLR.";
				return BAD_DEVICE_RESPONSE;
			}
		}
	}
	return OPERATION_SUCCESS;
}

int ConnectionPool::RestoreConnectionIfNeeded(unsigned int index, std::string& errDescription)
{
	if(!m_connected[index]) {
		logWriter.Write("Not connected to HLR", index);
		if (!Reconnect(index, errDescription)) {
			return NETWORK_ERROR;
		}
	}

	char recvBuf[receiveBufferSize];
	char sendBuf[sendBufferSize]; 
	int bytesRecv = recv(m_sockets[index], recvBuf, receiveBufferSize, 0) ;
	if (bytesRecv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)    {
		logWriter.Write("Error receiving data from host" + GetWinsockError(), index);
		if (!Reconnect(index, errDescription)) {
			return NETWORK_ERROR;
		}
	}
	if(bytesRecv > 0) {
		TelnetParse((unsigned char*) recvBuf, &bytesRecv, m_sockets[index]);
		recvBuf[bytesRecv]='\0';
		logWriter.Write(std::string("HLR initiated response: ") + recvBuf, index);
		_strupr_s(recvBuf, receiveBufferSize);
		if (strstr(recvBuf, "LOGGED OFF")) {
			logWriter.Write("LOGGED OFF message received, reconnecting ...", index);
			if (!Reconnect(index, errDescription)) {
				return NETWORK_ERROR;
			}
		}
		else if (strstr(recvBuf, "TIME OUT") || strstr(recvBuf, "CONNECTION INTERRUPTED")) {
			logWriter.Write("TIME OUT or CONNECTION INTERRUPTED report from HLR, restoring connection ...", index);
			sprintf_s(sendBuf, sendBufferSize, "\r\n");
			if(send(m_sockets[index],(char*) sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
				errDescription = "Socket error when sending restore connection message" + GetWinsockError();
				return NETWORK_ERROR;
			}
		}
	}
	return OPERATION_SUCCESS;
}


void ConnectionPool::WorkerThread(unsigned int index)
{
	while (!m_stopFlag) {
		std::unique_lock<std::mutex> locker(m_mutexes[index]);
		while(!m_stopFlag && !m_busy[index]) 
			m_condVars[index].wait(locker);
		if (!m_stopFlag && m_busy[index] && !m_finished[index]) {
			std::string errDescription;
			try {
				std::string deviceCommand;
				if (m_requests[index]->requestType == stateQuery) {
					deviceCommand = "MGSSP: IMSI=" + std::to_string(m_requests[index]->imsi) + ";";
				}
				else if (m_requests[index]->requestType == resetRequest) {
					deviceCommand = "HGSLR: IMSI=" + std::to_string(m_requests[index]->imsi) + ";";
				}
				else {
					// TODO: 
				}
				logWriter.Write("Sending request: " + deviceCommand, index);
				int res = SendCommandToDevice(index, deviceCommand, errDescription);
				if (res == OPERATION_SUCCESS) {
					res = ProcessDeviceResponse(index, VLR, m_requests[index]->requestType, deviceCommand, errDescription);
					if (m_requests[index]->requestType == stateQuery && res == INFO_NOT_COMPLETE) {
						logWriter.Write("VLR returned not complete information, sending request to HLR ...", index);
						// We have to check if subscriber is roaming. For that we will ask HLR for current VLR address
						deviceCommand = "HGSDP: IMSI=" + std::to_string(m_requests[index]->imsi) + ", LOC;";
						logWriter.Write("Sending request: " + deviceCommand, index);
						res = SendCommandToDevice(index, deviceCommand, errDescription);
						if (res == OPERATION_SUCCESS) {
							res = ProcessDeviceResponse(index, HLR, m_requests[index]->requestType, deviceCommand, errDescription);
						}
					}
				}
				logWriter.Write("Request processing finished.", index);
				m_resultCodes[index] = res;
				m_results[index] = errDescription;
			}
			catch(const std::exception& ex) {
				logWriter.Write(std::string("Exception caught: ") + ex.what(), index);
				m_resultCodes[index] = EXCEPTION_CAUGHT;
				m_results[index] = ex.what();
			}
			catch(...) {
				logWriter.Write("Unknown exception caught", index);
				m_resultCodes[index] = EXCEPTION_CAUGHT;
				m_results[index] = "Unknown exception";
			}
			m_finished[index] = true;
			m_condVars[index].notify_one();
		}
	}
}


bool ConnectionPool::TryAcquire(unsigned int& index)
{
	const int maxSecondsToAcquire = 2;
	int cycleCounter = 0;

	time_t startTime;
	time(&startTime);
	bool firstCycle = true;
	while (true) {
		// Start looping from last used connection + 1 to ensure consequent connections using and 
		// to avoid suspending rarely used connections
		for (int i = (firstCycle ? ((m_lastUsed + 1) %  m_config.connectionCount) : 0); i < m_config.connectionCount; ++i) {
			bool oldValue = m_busy[i];
			if (!oldValue) {
				if (m_busy[i].compare_exchange_weak(oldValue, true)) {
					index = i;
					m_finished[i] = false;
					m_lastUsed.store(i);
					return true;
				}
			}
		}
		firstCycle = false;
		++cycleCounter;
		if (cycleCounter > 1000) {
			// check time elapsed
			time_t now;
			time(&now);
			if (now - startTime > maxSecondsToAcquire)
				return false;
			cycleCounter = 0;
		}
	}
}


int8_t ConnectionPool::ExecRequest(unsigned int index, ClientRequest& clientRequest)
{
	m_requests[index] = &clientRequest;
	m_condVars[index].notify_one();
	
	std::unique_lock<std::mutex> locker(m_mutexes[index]);
	while (!m_finished[index]) {
		m_condVars[index].wait(locker);
	}
	int resultCode = m_resultCodes[index];
	clientRequest.resultDescr = m_results[index];
	m_busy[index] = false;
	return resultCode;
}


std::string ConnectionPool::GetWinsockError()
{
	return ". Error code: " + std::to_string(WSAGetLastError()) + ". " ;
}


