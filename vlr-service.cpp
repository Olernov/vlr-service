// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdarg.h>
#include <Winsock2.h>
#include "Common.h"
#include "Config.h"
#include "LogWriter.h"
#include "ConnectionPool.h"
#include "vlr-service.h"


LogWriter logWriter;


//int ExecuteRequest(uint64_t imsi, int nParamCount, char* pResult)
//{
//	try {
//		logWriter << "-----**********************----";
//		logWriter << std::string("ExecuteRequest for IMSI: ") + std::to_string(imsi);
//		
//		unsigned int connIndex;
//		logWriter.Write(std::string("Trying to acquire connection ... "), mainThreadIndex, debug);
//		if (!connectionPool.TryAcquire(connIndex)) {
//			logWriter << std::string("No free connection. Unable to execute request.");
//			return ALL_CONNECTIONS_BUSY;
//		}
//		logWriter.Write("Acquired connection #" + std::to_string(connIndex));
//		return connectionPool.ExecRequest(connIndex, imsi, pResult);
//	}
//	catch(const std::exception& ex) {
//		strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, ex.what(), strlen(ex.what()) + 1);
//		return EXCEPTION_CAUGHT;
//	}
//}




void TestCommandSender(int index, int commandsNum, int minSleepTime)
{
	srand((unsigned int)time(NULL));
	logWriter.Write(std::string("Started test command sender thread #") + std::to_string(index));
	for (int i = 0; i < commandsNum; ++i) {
		char* task = new char[50];
		char* result = new char[MAX_DMS_RESPONSE_LEN];
		switch (i % 3) {
		case 0:
			sprintf_s(task, 50, "HGSDC:MSISDN=79047186560,SUD=CLIP-%d;", rand() % 5);
			break;
		case 1:
			sprintf_s(task, 50, "HGSDP:MSISDN=79047172074,LOC;");
			break;
		case 2:
			sprintf_s(task, 50, "MGSSP:IMSI=250270100520482;");
			break;
		}
		result[0] = '\0';
		int res;
//		res = ExecuteCommand(&task, NUM_OF_EXECUTE_COMMAND_PARAMS, result);
		std::this_thread::sleep_for(std::chrono::seconds(minSleepTime + rand() % 3));
		delete [] task;
		delete [] result;
	}
}

void printUsage(char* programName)
{
    std::cerr << "Usage: " << std::endl << programName << " <config-file> [-test]" << std::endl;
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    const char* confFilename = argv[1];
    bool runTests = false;
    if (argc > 2 && !strncasecmp(argv[2], "-test", 5)) {
        runTests = true;
    }
    std::ifstream confFile(confFilename, std::ifstream::in);
    if (!confFile.is_open()) {
        std::cerr << "Unable to open config file " << confFilename << std::endl;
        exit(EXIT_FAILURE);
    }
	Config config;
	try {
        config.ReadConfigFile(confFile);
        config.ValidateParams();
    }
    catch(const std::exception& ex) {
        std::cerr << "Error when parsing config file " << confFilename << " " << std::endl;
        std::cerr << ex.what() <<std::endl;
        exit(EXIT_FAILURE);
    }
#ifndef _WIN32
    const std::string pidFilename = "/var/run/pgw-aggregator.pid";
    std::ofstream pidFile(pidFilename, std::ofstream::out);
    if (pidFile.is_open()) {
        pidFile << getpid();
    }
    pidFile.close();
#endif
	
	try {
		logWriter.Initialize(config.logDir, "vlr", config.logLevel);
		logWriter << "VLR service start. Configuration settings:";
		logWriter << config.DumpAllSettings();

		ConnectionPool connectionPool(config);
		std::string errDescription;
		if (!connectionPool.Initialize(config, errDescription)) {
			std::cerr << "Unable to initialize connection pool: " << errDescription << ". Exiting." << std::endl;
			exit(EXIT_FAILURE);
		}
		std::cout << "VLR service started. See log files at LOG_DIR for further details" << std::endl;
		
		if (runTests) {
			char c;
			while (true) {
				std::cout << "Choose IMSI: 1-2 - correct, 3 - unregistered, 4 - roaming, 9 - wrong (too long), q - quit: ";
				std::cin >> c;
				if (c == 'q' || c == 'Q') {
					break;
				}
				RequestType requestType;
				uint64_t imsi;
				switch (c) {
				case '1':
					requestType = stateQuery;
					imsi = 250270100520482;
					break;
				case '2':
					requestType = stateQuery;
					imsi = 250270100293873;
					break;
				case '3':
					requestType = stateQuery;
					imsi = 250270100273000;
					break;
				case '4':
					requestType = stateQuery;
					imsi = 250270100604804;
					break;
				case '9':
					requestType = stateQuery;
					imsi = 25027010052048200;
					break;
				default:
					std::cout << "Wrong option entered, try again" << std::endl;
					continue;
				}
				unsigned int connIndex;
				if (!connectionPool.TryAcquire(connIndex)) {
					std::cout << "Unable to acqure connection for request execution." << std::endl;
					continue;
				}
				std::cout << "Acquired connection #" + std::to_string(connIndex) << std::endl;
				std::string resultDescr;
				int requestRes = connectionPool.ExecRequest(connIndex, imsi, requestType, resultDescr);
				std::cout << "Result: " << requestRes << " (" << resultDescr << ")" << std::endl;
			}
			/*std::cout << "Running multithreaded test in " << config.connectionCount << " threads ..." << std::endl;
			std::vector<std::thread> cmdSenderThreads;
			for (int i = 0; i < config.connectionCount; i++) {
				cmdSenderThreads.push_back(std::thread(TestCommandSender, i, 20, 1));
				// run next thread after a random time-out
				std::this_thread::sleep_for(std::chrono::seconds(rand() % 2));
			}
			for (auto& thr : cmdSenderThreads) {
				thr.join();
			}
			std::cout << "*****************************" << std::endl;
			std::cout << "Multi-threaded test PASSED. Check log file written at logpath." << std::endl;
			std::cout << "Running reconnect test ..." << std::endl;
			std::thread reconnectTest(TestCommandSender, 0, 3, 310);
			reconnectTest.join();
			std::cout << "*****************************" << std::endl;
			std::cout << "Reconnect test PASSED. Check log file. There must be a message like \"Restoring connection\"" << std::endl;*/
		}
		
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() <<  ". Exiting." <<std::endl;
		logWriter << std::string(ex.what()) +  ". Exiting.";
	}
	logWriter << "Closing connections and stopping.";
	
#ifndef _WIN32
	filesystem::remove(pidFilename);
#endif
}