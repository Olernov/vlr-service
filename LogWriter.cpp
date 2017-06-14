#include <iostream>
#include <string>
#include "LogWriter.h"


LogWriter::LogWriter() 	:
	m_initialized(false),
    messageQueue(queueSize),
    m_stopFlag(false),
    m_excPointer(nullptr),
    m_writeThread(&LogWriter::WriteThreadFunction, this)
{
}

bool LogWriter::Initialize(const std::string& logPath, const std::string& namePrefix, LogLevel level)
{
    m_logPath = logPath;
	m_namePrefix = namePrefix;
    logLevel = level;
	time_t now;
	time(&now);
    SetLogStream(now);
	m_initialized = true;
	return true;
}


void LogWriter::WriteThreadFunction()
{
	while (true) {
		try {
			if (m_stopFlag && messageQueue.empty()) {
				return;
			}
			if (!m_initialized || messageQueue.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(sleepWhenQueueEmpty));
            }
			while (!messageQueue.empty()) {
				LogMessage* pMessage;
				if (messageQueue.pop(pMessage)) {
					tm messageTime;
                    localtime_r(&pMessage->m_time, &messageTime);
					char timeBuf[30];
					strftime(timeBuf, 29, "%H:%M:%S", &messageTime);
                    SetLogStream(pMessage->m_time);
					m_logStream << timeBuf << "  |  "
                        << (pMessage->m_threadNum != mainThreadIndex ? std::to_string(pMessage->m_threadNum) : " ")
						<< "  |  " << pMessage->m_message.c_str() << std::endl;
					delete pMessage;
				}
			}
		}
		catch (...) {
			std::lock_guard<std::mutex> lock(m_exceptionMutex);
			if (m_excPointer == nullptr)
				// if exception is not set or previous exception is cleared then set new
				m_excPointer = std::current_exception();
		}
	}
}


void LogWriter::SetLogStream(time_t messageTime)
{
    tm messageTimeTm;
	char dateBuf[30];
	localtime_r(&messageTime, &messageTimeTm);
	snprintf(dateBuf, 30, "%4.4d%2.2d%2.2d", messageTimeTm.tm_year+1900, messageTimeTm.tm_mon+1, messageTimeTm.tm_mday);
	if(std::string(dateBuf) != m_logFileDate || !m_logStream.is_open()) {
        m_logStream.close();
        char logName[maxPath];
        if(!m_logPath.empty()) {
            snprintf(logName, maxPath, "%s/%s_%s.log", m_logPath.c_str(), m_namePrefix.c_str(), dateBuf);
        }
        else {
            snprintf(logName, maxPath, "./%s_%s.log", m_namePrefix.c_str(), dateBuf);
        }
		m_logStream.open(logName, std::fstream::app | std::fstream::out);
        if (!m_logStream.is_open()) {
			throw std::runtime_error(std::string("Unable to open log file ") + std::string(logName));
        }
		m_logFileDate = dateBuf;
	}
}



bool LogWriter::Write(LogMessage* message)
{
	try {
        if (!messageQueue.push(message))
			throw LogWriterException("Unable to add message to log queue");
	}
	catch(...){
		std::cerr << "exception when LogWriter::Write" << std::endl;
	}
	return true;
}

bool LogWriter::Write(std::string message, short threadIndex, LogLevel msgLevel)
{
    if (msgLevel >= logLevel) {
        time_t now;
        time(&now);
        LogMessage* pnewMessage = new LogMessage(now, threadIndex, message);
        Write(pnewMessage);
    }
	return true;
}

void LogWriter::operator<<(const std::string& message)
{
    Write(message, mainThreadIndex);
}


void LogWriter::ClearException()
{
	std::lock_guard<std::mutex> lock(m_exceptionMutex);
	m_excPointer = nullptr;
}

LogWriter::~LogWriter()
{
    m_stopFlag = true;
	if (m_writeThread.joinable()) {
		m_writeThread.join();
	}
	m_logStream.close(); 
}
