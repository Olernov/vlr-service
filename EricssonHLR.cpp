// EricssonHLR.cpp : Defines the entry point for the console application.
//

 //#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <stdarg.h>
#include <Winsock2.h>
#include "Common.h"
#include "ConfigContainer.h"
#include "LogWriter.h"
#include "ConnectionPool.h"

Config config;
LogWriter logWriter;
ConnectionPool connectionPool;
mutex g_coutMutex;

__declspec (dllexport) int __stdcall InitService(char* szInitParams, char* szResult)
{
	try {
		string errDescription;
		if (!config.ReadConfigString(szInitParams, errDescription)) {
			strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length() + 1);
			return ERROR_INIT_PARAMS;
		}
		if (!config.m_ignoredMsgFilename.empty()) {
			if (!config.ReadIgnoredMsgFile(errDescription)) {
				strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length() + 1);
				return ERROR_INIT_PARAMS;
			}
		}
		if (!logWriter.Initialize(config.m_logPath, "EricssonHLR")) {
			strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length() + 1);
			return INIT_FAIL;
		}
		logWriter.Write("***** Starting Ericsson HLR driver v2.0 (multithreaded) *****");
		logWriter.Write(string("Original init string: ") + string(szInitParams));
		logWriter.Write(string("Parsed init params: "));
		logWriter.Write(string("   Host: ") + config.m_hostName);
		logWriter.Write(string("   Port: ") + to_string(config.m_port));
		logWriter.Write(string("   Username: ") + config.m_username);
		logWriter.Write(string("   Password: ") + config.m_password);
		logWriter.Write(string("   Domain: ") + config.m_domain);
		logWriter.Write(string("   Log path: ") + config.m_logPath);
		logWriter.Write(string("   Ignored HLR messages file: ") + config.m_ignoredMsgFilename);
		logWriter.Write(string("   Number of threads: ") + to_string(config.m_numThreads));
		logWriter.Write(string("   Debug mode: ") + to_string(config.m_debugMode));

		if(!connectionPool.Initialize(config, errDescription)) {
			logWriter.Write("Unable to set up connection to given host: " + errDescription);
			strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, errDescription.c_str(), errDescription.length() + 1);
			return ERROR_INIT_PARAMS;
		}
	}
	catch(LogWriterException& e) {
		strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, e.m_message.c_str(), e.m_message.length() + 1);
		connectionPool.Close();
		return INIT_FAIL;
	}
	catch(std::exception& e)	{
		//const char* pmessage = "Exception caught while trying to initialize.";
		strncpy_s(szResult, MAX_DMS_RESPONSE_LEN, e.what(), strlen(e.what()) +1);
		connectionPool.Close();
		return INIT_FAIL;
	}
	return OPERATION_SUCCESS;
}


__declspec (dllexport) int __stdcall ExecuteCommand(char **pParam, int nParamCount, char* pResult)
{
	try {
		char* pCommand = pParam[0];
		logWriter << "-----**********************----";
		logWriter << string("ExecuteCommand: ") + pCommand;
		if (strlen(pCommand) > MAX_COMMAND_LEN) {
			strcpy_s(pResult, MAX_DMS_RESPONSE_LEN, "Command is too long.");
			logWriter << "Error: Command length exceeds 1020 symbols. Won't send to HLR.";
			return ERROR_CMD_TOO_LONG;
		}
		// check logwriter exception
		if (logWriter.GetException() != nullptr) {
			try {
				rethrow_exception(logWriter.GetException());
			}
			catch (const exception& ex) {
				strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, ex.what(), strlen(ex.what()) + 1);
				logWriter << "LogWriter exception detected. Command won't execute until fixed";
				logWriter.ClearException();
				return EXCEPTION_CAUGHT;
			}
		}

		unsigned int connIndex;
		logWriter.Write(string("Trying to acquire connection for: ") + pCommand);
		if (!connectionPool.TryAcquire(connIndex)) {
			logWriter << string("No free connection for: ") + pCommand;
			logWriter << "Sending TRY_LATER result";
			return TRY_LATER;
		}
		logWriter.Write("Acquired connection #" + to_string(connIndex) + " for: " + pCommand);
		return connectionPool.ExecCommand(connIndex, pParam[0], pResult);
	}
	catch(const exception& ex) {
		strncpy_s(pResult, MAX_DMS_RESPONSE_LEN, ex.what(), strlen(ex.what()) + 1);
		return EXCEPTION_CAUGHT;
	}
}


__declspec (dllexport) int __stdcall DeInitService(char* szResult)
{
	logWriter << "DeInitService called. Closing connections and stopping.";
	connectionPool.Close();
	return OPERATION_SUCCESS;
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL,  DWORD fdwReason,  LPVOID lpvReserved)
{
	switch ( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			// A process is creating a new thread.
		break;
		case DLL_THREAD_DETACH:
			// A thread exits normally.
			break;
		case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

void TestCommandSender(int index, int commandsNum, int minSleepTime)
{
	srand((unsigned int)time(NULL));
	logWriter.Write(string("Started test command sender thread #") + to_string(index));
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
		res = ExecuteCommand(&task, NUM_OF_EXECUTE_COMMAND_PARAMS, result);
		this_thread::sleep_for(std::chrono::seconds(minSleepTime + rand() % 3));
		delete [] task;
		delete [] result;
	}
}

int main(int argc, char* argv[])
{
	// This is a test entry-point, because our target is DLL and main() is not exported.
	// Different tests may be implemented here. Set configuration type to Application (*.exe)
	// and run tests. If successful, set config to DLL and deploy it to DMS.

	char initResDescription[MAX_DMS_RESPONSE_LEN];
	int initRes = InitService(argv[1], initResDescription);
	cout << "InitService res: " << initRes << (initRes == OPERATION_SUCCESS ? " (SUCCESS)" : " (FAIL)") << endl;
	if (initRes != OPERATION_SUCCESS) {
		cout << "Description: " << initResDescription << endl;
		char dummy;
		cout << ">";
		std::cin >> dummy;
		return INIT_FAIL;
	}
	cout << "Running multithreaded test in " << config.m_numThreads << " threads ..." << endl;
	if (initRes == OPERATION_SUCCESS) {
		vector<thread> cmdSenderThreads;
		for (int i = 0; i < config.m_numThreads; i++) {
			cmdSenderThreads.push_back(thread(TestCommandSender, i, 20, 1));
			// run next thread after a random time-out
			this_thread::sleep_for(std::chrono::seconds(rand() % 2));
		}
		for(auto& thr : cmdSenderThreads) {
			thr.join();
		}
	}
	cout << "*****************************" << endl;
	cout << "Multithreaded test PASSED. Check log file written at logpath." << endl;

	cout << "Running reconnect test ..." << endl;
	thread reconnectTest(TestCommandSender, 0, 3, 310);
	reconnectTest.join();
	cout << "*****************************" << endl;
	cout << "Reconnect test PASSED. Check log file. There must be a message like \"Restoring connection\"" << endl;

	int deinitRes;
	char deinitResDescription[MAX_DMS_RESPONSE_LEN];
	deinitRes = DeInitService(deinitResDescription);
	cout << "DeinitService result: " << deinitRes << (deinitRes == OPERATION_SUCCESS ? " (SUCCESS)" : " (FAIL)") << endl;
	if (deinitRes != OPERATION_SUCCESS) {
		cout << "Description: " << initResDescription << endl;
	}
	char dummy;
	cout << ">";
	std::cin >> dummy;
	return OPERATION_SUCCESS;
}