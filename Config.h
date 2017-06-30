#pragma once
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "LogWriter.h"

struct Config
{
public:
    Config();
    Config(std::ifstream& cfgStream);

    void ReadConfigFile(std::ifstream& cfgStream);
    void ValidateParams();
    std::string DumpAllSettings();

    std::string vlrAddr;
	unsigned int vlrPort;
	unsigned int serverPort;
    std::string username;
    std::string password;
    std::string domain;
	std::string logDir;
    std::string cdrExtension;
	unsigned short connectionCount;
    LogLevel logLevel;
private:
    const std::string vlrAddrParamName = "VLR_ADDRESS";
    const std::string vlrPortParamName = "VLR_PORT";
    const std::string usernameParamName = "VLR_USERNAME";
    const std::string passwordParamName = "VLR_PASSWORD";
    const std::string logDirParamName = "LOG_DIR";
    const std::string connectionCountParamName = "CONNECTION_COUNT";
    const std::string logLevelParamName = "LOG_LEVEL";
    const std::string serverPortParamName = "SERVER_PORT";
    const int minConnCount = 1;
    const int maxConnCount = 16;
    unsigned long ParseULongValue(const std::string& name, const std::string& value);
};
