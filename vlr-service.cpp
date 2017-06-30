#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
	#include <Winsock2.h>
#else
    #include <signal.h>
    #include <unistd.h>
#endif
#include "Common.h"
#include "Config.h"
#include "LogWriter.h"
#include "ConnectionPool.h"
#include "Server.h"
#include "vlr-service.h"


LogWriter logWriter;
Server server;

void CloseSocket(int socket)
{
#ifdef WIN32
	shutdown(socket, SD_BOTH);
	closesocket(socket);
#else
	shutdown(socket, SHUT_RDWR);
    close(socket);
#endif
}

#ifndef _WIN32
void SignalHandler(int signum, siginfo_t *info, void *ptr)
{
    std::cout << "Received signal #" <<signum << " from process #" << info->si_pid << ". Stopping ..." << std::endl;
    server.Stop();
}
#endif


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
		std::this_thread::sleep_for(std::chrono::seconds(minSleepTime + rand() % 3));
		delete [] task;
		delete [] result;
	}
}

void printUsage(char* programName)
{
    std::cerr << "Usage: " << std::endl << programName << " <config-file> [-test]" << std::endl;
}

void RunKeyboardTests(ConnectionPool& connectionPool)
{
	char c;
	while (true) {
		std::cout << "Choose IMSI: 1-2 - correct, 3 - unregistered, 4 - roaming, 9 - wrong (too long), q - quit: ";
		std::cin >> c;
		if (c == 'q' || c == 'Q') {
			break;
		}
		
		ClientRequest clientRequest(0);
		clientRequest.requestNum = 1;
		switch (c) {
		case '1':
			clientRequest.requestType = stateQuery;
			clientRequest.imsi = 250270100520482;
			break;
		case '2':
			clientRequest.requestType = stateQuery;
			clientRequest.imsi = 250270100307757;
			break;
		case '3':
			clientRequest.requestType = stateQuery;
			clientRequest.imsi = 250270100273000;
			break;
		case '4':
			clientRequest.requestType = stateQuery;
			clientRequest.imsi = 250270100604804;
			break;
		case '9':
			clientRequest.requestType = stateQuery;
			clientRequest.imsi = 25027010052048200;
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
		
		int requestRes = connectionPool.ExecRequest(connIndex, clientRequest);
		std::cout << "Result: " << requestRes << " (" << resultDescr << ")" << std::endl;
	}
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
    const std::string pidFilename = "/var/run/vlr-service.pid";
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
				
        if (!server.Initialize(config.serverPort, &connectionPool, errDescription)) {
            std::cerr << "Unable to initialize server: " << errDescription << ". Exiting." << std::endl;
            exit(EXIT_FAILURE);
        }
#ifndef _WIN32
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        act.sa_sigaction = SignalHandler;
        act.sa_flags = SA_SIGINFO;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGTERM, &act, NULL);
#endif
		std::cout << "VLR service started. See log files at LOG_DIR for further details" << std::endl;
		if (runTests) {
			RunKeyboardTests(connectionPool);
		}
		else {
			server.Run();
		}
		
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() <<  ". Exiting." <<std::endl;
		logWriter << std::string(ex.what()) +  ". Exiting.";
	}
	logWriter << "Closing connections and stopping.";
#ifdef _WIN32
	WSACleanup();
#endif

#ifndef _WIN32
    remove(pidFilename.c_str());
#endif
}
