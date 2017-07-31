#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Common.h"
#include "ResponseParser.h"
#include "Config.h"


class HLRConnector
{
public:
    HLRConnector(unsigned int thisIndex, const Config& config);
    ~HLRConnector();
    bool Initialize();
    bool ProcessCommand(const std::string& command, std::string& response);
    inline std::string& GetErrDescription() { return errDescription; }
private:
    static const int receiveBufferSize = 65000;
    static const int sendBufferSize = 1024;
    static const int maxLoginAttempts = 5;
    static const int socketTimeoutSec = 5;
    const char* terminalType = "vt100";

    unsigned int thisIndex;
    int hlrSocket;
    bool socketConnected;
    bool loggedInToHLR;
    const Config& config;
    bool stopFlag;
    std::string errDescription;

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

    bool CreateSocketAndConnect();
    bool LoginToHLR();
    bool MakeAttemptToLogin();
    bool SendLoginAttribute(const std::string& attributeName, const std::string& attributeValue);
    bool Reconnect();
    void RestoreConnectionIfNeeded();
    void TelnetParse(unsigned char* recvbuf, int* bytesRecv);
    bool SendCommandToDevice(std::string hlrCommand);
    bool ProcessDeviceResponse(std::string& response);
    std::string GetWinsockError();
};

