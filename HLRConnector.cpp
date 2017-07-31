#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <chrono>
#include <thread>
#include "Config.h"
#include "HLRConnector.h"
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
#endif


extern LogWriter logWriter;
extern void CloseSocket(int socket);

HLRConnector::HLRConnector(unsigned int index, const Config& config) :
    thisIndex(index),
    socketConnected(false),
    loggedInToHLR(false),
    config(config),
    stopFlag(false)
{
}


HLRConnector::~HLRConnector()
{
    CloseSocket(hlrSocket);
}


bool HLRConnector::Initialize()
{
#ifdef WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2,2), &wsaData)) {
    errDescription = "Error initializing Winsock: " + std::to_string(WSAGetLastError());
            return false;
    }
#endif
    if (!CreateSocketAndConnect()) {
        return false;
    }
    socketConnected = true;
    logWriter.Write("Connected to HLR successfully.", thisIndex);
		
    if (!LoginToHLR()) {
        return false;
    }
    return true;
}


bool HLRConnector::CreateSocketAndConnect()
{
    struct sockaddr_in addr;
    hlrSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hlrSocket == INVALID_SOCKET) {
        errDescription = "Error creating socket. " + GetWinsockError();
        return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(config.vlrAddr.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        errDescription = "Error parsing host address: " + config.vlrAddr + GetWinsockError();
        return false;
    }
    addr.sin_port = htons(config.vlrPort);
    socketConnected = false;
    if(connect(hlrSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        errDescription = "Unable to connect to host " + config.vlrAddr + GetWinsockError();
        return false;
    }
    u_long iMode=1;
    if(ioctlsocket(hlrSocket, FIONBIO, &iMode) != 0) {
        errDescription = "Error setting socket in non-blocking mode. " + GetWinsockError();
        return false;
    }
    socketConnected = true;
    return true;
}


bool HLRConnector::LoginToHLR()
{
    loggedInToHLR = false;
    int nAttemptCounter = 0;
    do {
        logWriter.Write("Login attempt " + std::to_string(nAttemptCounter+1), thisIndex, debug);
        loggedInToHLR = MakeAttemptToLogin();
	}
    while(!loggedInToHLR && ++nAttemptCounter < maxLoginAttempts);
    return loggedInToHLR;
}


bool HLRConnector::MakeAttemptToLogin()
{
    fd_set read_set;
    struct timeval tv;
    char recvbuf[receiveBufferSize];

    while(true) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&read_set);
        FD_SET(hlrSocket, &read_set);
        if (select(hlrSocket + 1, &read_set, NULL, NULL, &tv ) != 0 ) {
            if (FD_ISSET(hlrSocket, &read_set))  {
                recvbuf[0] = STR_TERMINATOR;
                int bytesRecv = recv(hlrSocket, recvbuf, sizeof(recvbuf), 0);
                if (bytesRecv == SOCKET_ERROR)  {
                    errDescription = "LoginToHLR: Error receiving data from host" + GetWinsockError();
                    return false;
                }

                TelnetParse((unsigned char*)recvbuf, &bytesRecv);
                if (bytesRecv>0) {
                    recvbuf[bytesRecv] = STR_TERMINATOR;
                    logWriter.Write("LoginToHLR: HLR response: " + std::string(recvbuf), thisIndex, debug);
                    _strupr_s(recvbuf, receiveBufferSize);

                    if(strstr(recvbuf, "LOGIN:")) {
                        if (!SendLoginAttribute("login", config.username)) {
                            return false;
                        }
                        continue;
                    }
                    if(strstr(recvbuf,"PASSWORD:")) {
                        if (!SendLoginAttribute("password", config.password)) {
                            return false;
                        }
                        continue;
                    }
                    if(strstr(recvbuf,"DOMAIN:")) {
                        if (!SendLoginAttribute("domain", config.domain)) {
                            return false;
                        }
                        continue;
                    }
                    if(strstr(recvbuf,"TERMINAL TYPE?")) {
                        if (!SendLoginAttribute("terminal type", terminalType)) {
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
        else {
            logWriter.Write("LoginToHLR: select time-out", thisIndex, debug);
            return false;
        }
    }
}


bool HLRConnector::SendLoginAttribute(const std::string& attributeName, const std::string& attributeValue)
{
    logWriter.Write("Sending " + attributeName + ": " + attributeValue, thisIndex, debug);
    char sendbuf[sendBufferSize];
    sprintf_s(sendbuf, sendBufferSize, "%s\r\n", attributeValue.c_str());
    if(send(hlrSocket, sendbuf, strlen(sendbuf), 0) == SOCKET_ERROR) {
        errDescription = "Error sending data on socket" + GetWinsockError();
        return false;
    }
    return true;
}


bool HLRConnector::Reconnect()
{
    logWriter.Write("Trying to reconnect ...", thisIndex);
    CloseSocket(hlrSocket);
    socketConnected = false;
    loggedInToHLR = false;
    if(CreateSocketAndConnect()) {
        if(LoginToHLR()) {
            logWriter.Write("Reconnected and logged in successfully", thisIndex);
            socketConnected = true;
            loggedInToHLR = true;
			return true;
		}
	}
    logWriter.Write("Unable to reconnect.", thisIndex);
	return false;
}


bool HLRConnector::ProcessCommand(const std::string& command, std::string& response)
{
    if (!SendCommandToDevice(command)) {
        return false;
    }
    return ProcessDeviceResponse(response);
}


bool HLRConnector::SendCommandToDevice(std::string command)
{
    RestoreConnectionIfNeeded();
    if (!loggedInToHLR) {
        return false;
	}
	char sendBuf[sendBufferSize];
    sprintf_s(sendBuf, sendBufferSize, "%s\r\n", command.c_str());
    if (send(hlrSocket, (char*)sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
		errDescription = "Socket error when sending command" + GetWinsockError();
        return false;
	}
    return true;
}


bool HLRConnector::ProcessDeviceResponse(std::string& response)
{
	char recvBuf[receiveBufferSize];
	char hlrResponse[receiveBufferSize];
	hlrResponse[0] = STR_TERMINATOR;
	fd_set read_set;
	struct timeval tv;
    while (true) {
        tv.tv_sec = socketTimeoutSec;
		tv.tv_usec = 0;
		FD_ZERO(&read_set);
        FD_SET(hlrSocket, &read_set);
        if (select(hlrSocket + 1, &read_set, NULL, NULL, &tv) != 0) {
			// check for message
            if (FD_ISSET(hlrSocket, &read_set)) {
				// receive some data from server
                int bytesRecv = recv(hlrSocket, recvBuf, receiveBufferSize, 0);
				if (bytesRecv == SOCKET_ERROR)    {
					errDescription = "Socket error when receiving data" + GetWinsockError();
                    return false;
				}

                TelnetParse((unsigned char*)recvBuf, &bytesRecv);
                if (bytesRecv > 0) {
                    recvBuf[bytesRecv] = STR_TERMINATOR;
                    logWriter.Write(std::string("HLR response: ") + recvBuf, thisIndex, debug);
                    _strupr_s(recvBuf, receiveBufferSize);
                    strncat(hlrResponse, recvBuf, bytesRecv + 1);

                    if (strstr(hlrResponse, "END") != nullptr || strstr(hlrResponse, "EXECUTED") != nullptr) {
                        response.assign(hlrResponse);
                        return true;
                    }
                    else {
                        char* p = strstr(hlrResponse, "NOT ACCEPTED");
                        if (p != nullptr) {
                            ResponseParser::StripHLRResponse(p, response);
                            errDescription = response;
                            return false;
                        }
                    }
                }
                else {
                    logWriter.Write("No bytes to read", thisIndex, debug);
                    if (strlen(hlrResponse)) {
                        errDescription = std::string("Unable to parse VLR/HLR response:\n") + hlrResponse;
                        return false;
                    }
                    else {
                        errDescription = "No response received from VLR/HLR.";
                        return false;
                    }
                }
			}
		}
		else {
            logWriter.Write("ExecuteCommand: socket time-out", thisIndex, debug);
			if (strlen(hlrResponse) > 0) {
				errDescription = std::string("Unable to parse HLR response:\n") + hlrResponse;
                return false;
			}
			else {
				errDescription = "No response received from HLR.";
                return false;
			}
		}
	}
    return true;
}


void HLRConnector::RestoreConnectionIfNeeded()
{
    if(!socketConnected) {
        logWriter.Write("Not connected to HLR", thisIndex);
        if (!Reconnect()) {
            return;
        }
	}

	char recvBuf[receiveBufferSize];
	char sendBuf[sendBufferSize]; 
    int bytesRecv = recv(hlrSocket, recvBuf, receiveBufferSize, 0) ;
	if (bytesRecv == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)    {
        logWriter.Write("Error receiving data from host" + GetWinsockError(), thisIndex);
        if (!Reconnect()) {
            return;
		}
	}
	if(bytesRecv > 0) {
        TelnetParse((unsigned char*) recvBuf, &bytesRecv);
        recvBuf[bytesRecv] = STR_TERMINATOR;
        logWriter.Write(std::string("HLR initiated response: ") + recvBuf, thisIndex, debug);
		_strupr_s(recvBuf, receiveBufferSize);
		if (strstr(recvBuf, "LOGGED OFF")) {
            logWriter.Write("LOGGED OFF message received, reconnecting ...", thisIndex);
            if (!Reconnect()) {
                return;
			}
		}
		else if (strstr(recvBuf, "TIME OUT") || strstr(recvBuf, "CONNECTION INTERRUPTED")) {
            logWriter.Write("TIME OUT or CONNECTION INTERRUPTED report from HLR, restoring connection ...",
                            thisIndex);
			sprintf_s(sendBuf, sendBufferSize, "\r\n");
            if(send(hlrSocket,(char*) sendBuf, strlen(sendBuf), 0) == SOCKET_ERROR) {
				errDescription = "Socket error when sending restore connection message" + GetWinsockError();
                return;
			}
		}
	}
    return;
}


std::string HLRConnector::GetWinsockError()
{
	return ". Error code: " + std::to_string(WSAGetLastError()) + ". " ;
}


// This code is taken from NetCat project http://netcat.sourceforge.net/
void HLRConnector::TelnetParse(unsigned char* recvbuf, int* bytesRecv)
{
    /*static*/ unsigned char getrq[4];
    unsigned char putrq[4], *buf=recvbuf;
    int eat_chars=0;
    int l = 0;
    /* loop all chars of the string */
    for(int i=0; i<*bytesRecv; i++)
    {
         if (recvbuf[i]==0) {
            // sometimes there are 0 chars in HLR responses. Replace them with space
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
          send( hlrSocket, (char*)putrq,3, 0 );
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
          send( hlrSocket, (char*)putrq,3, 0 );
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
