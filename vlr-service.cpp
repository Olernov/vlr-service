// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
	#include <Winsock2.h>
#endif
#include "Common.h"
#include "Config.h"
#include "LogWriter.h"
#include "ConnectionPool.h"
#include "Server.h"
#include "vlr-service.h"


LogWriter logWriter;

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

//
//bool ProcessIncomingConnection()
//{
//    #ifdef WIN32
//      int clilen;
//    #else
//      socklen_t clilen;
//    #endif
//    struct sockaddr_in newclient_addr;
//    char address_buffer[64];
//    clilen = sizeof(newclient_addr);
//    int newsockfd = accept(listen_socket, (struct sockaddr *)&newclient_addr, &clilen);
//    if (newsockfd < 0) {
//       log("Failed to call accept on socket: %d", SOCK_ERR);
//       return false;
//    }
//
//    log("Incoming connection from %s",IPAddr2Text(&newclient_addr.sin_addr, address_buffer, sizeof(address_buffer)));
//
//    int freeSocketIndex = NO_FREE_CONNECTIONS;
//    for(int i=0; i < gwOptions.max_connections; i++) {
//        if(client_socket[i] == 0) {
//            freeSocketIndex = i;
//            break;
//        }
//    }
//    if (freeSocketIndex == NO_FREE_CONNECTIONS || bShutdownInProgress) {
//        if(freeSocketIndex == NO_FREE_CONNECTIONS) {
//            log("Maximum allowed connections reached. Rejecting connection...");
//        }
//        else {
//            log("Shutdown in progress. Rejecting connection...");
//        }
//        CloseSocket(newsockfd);
//        return false;
//    }
//    if(gwOptions.allowed_clients_num > 0) {
//        bool bConnectionAllowed = false;
//        // check connection for white list of allowed clients
//        for(int j=0; j < gwOptions.allowed_clients_num; j++)
//            if(!strcmp(gwOptions.allowed_IP[j], address_buffer)) {
//                bConnectionAllowed=true;
//                break;
//            }
//        if(!bConnectionAllowed) {
//            log("Client address %s not found in white list of allowed IPs. Rejecting connection...",address_buffer);
//            CloseSocket(newsockfd);
//            return false;
//        }
//    }
//
//    client_socket[freeSocketIndex] = newsockfd;
//    memcpy(&client_addr[freeSocketIndex], &newclient_addr, sizeof(newclient_addr));
//    log("Connection #%d accepted.", freeSocketIndex);
//    return true;
//}
//
//bool ProcessSocketEvents()
//{
//    struct timeval tv;
//    tv.tv_sec = 0;  // time-out
//    tv.tv_usec = 100;
//    fd_set read_set, write_set;
//    FD_ZERO( &read_set );
//    FD_ZERO( &write_set );
//    FD_SET( listen_socket, &read_set );
//    int max_socket = listen_socket;
//    for(size_t i = 0; i < gwOptions.max_connections; i++) {
//        if(client_socket[i] != FREE_SOCKET) {
//            FD_SET( client_socket[i], &read_set );
//            if(client_socket[i] > max_socket)
//                max_socket=client_socket[i];
//        }
//    }
//    const int SELECT_TIMEOUT = 0;
//    int socketCount;
//    if ( (socketCount = select( max_socket + 1, &read_set, &write_set, NULL, &tv )) != SELECT_TIMEOUT) {
//        if(socketCount == SOCKET_ERROR) {
//            log("select function returned error: %d", SOCK_ERR);
//            return false;
//        }
//        if(FD_ISSET(listen_socket, &read_set)) {
//            ProcessIncomingConnection();
//        }
//        for(int sockIndex=0; sockIndex < gwOptions.max_connections; sockIndex++) {
//          if(FD_ISSET(client_socket[sockIndex], &read_set)) {
//              if (!ProcessIncomingData(sockIndex)) {
//                  log("Error %d receiving data on connection #%d. Closing connection..." , SOCK_ERR, sockIndex);
//                  CloseSocket(client_socket[sockIndex]);
//                  client_socket[sockIndex] = FREE_SOCKET;
//                  continue;
//              }
//          }
//       }
//    }
//    return true;
//}


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
				
		Server server(config.serverPort, connectionPool);
				
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
	filesystem::remove(pidFilename);
#endif
}