#include "CX_Logger.h"

/*! This is an instance of CX::CX_Logger that is hooked into the CX backend.
All log messages generated by CX and openFrameworks go through this instance.
After runExperiment() returns, CX::Instances::Log.flush() is called.
\ingroup entryPoint
*/
CX::CX_Logger CX::Instances::Log;

namespace CX {
namespace Private {

	enum class LogTarget {
		CONSOLE,
		FILE,
		CONSOLE_AND_FILE
	};

	struct CX_LoggerTargetInfo {
		CX_LoggerTargetInfo(void) :
			file(nullptr)
		{}

		LogTarget targetType;
		CX_LogLevel level;

		std::string filename;
		ofFile *file;
	};

	struct CX_LogMessage {
		CX_LogMessage(CX_LogLevel level_, string module_) :
			level(level_),
			module(module_)
		{}

		std::string message;
		CX_LogLevel level;
		std::string module;
		std::string timestamp;
	};

	struct CX_ofLogMessageEventData_t {
		ofLogLevel level;
		std::string module;
		std::string message;
	};

	//////////////////////
	// CX_LoggerChannel //
	//////////////////////

	class CX_LoggerChannel : public ofBaseLoggerChannel {
	public:
		~CX_LoggerChannel(void) {};

		void log(ofLogLevel level, const std::string & module, const std::string & message);
		void log(ofLogLevel level, const std::string & module, const char* format, ...);
		void log(ofLogLevel level, const std::string & module, const char* format, va_list args);

		ofEvent<CX_ofLogMessageEventData_t> messageLoggedEvent;
	};

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const std::string & message) {
		CX_ofLogMessageEventData_t md;
		md.level = level;
		md.module = module;
		md.message = message;
		ofNotifyEvent(this->messageLoggedEvent, md);
	}

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const char* format, ...) {
		va_list args;
		va_start(args, format);
		this->log(level, module, format, args);
		va_end(args);
	}

	void CX_LoggerChannel::log(ofLogLevel level, const std::string & module, const char* format, va_list args) {
		int bufferSize = 256;

		char *buffer = new char[bufferSize];

		while (true) {

			int result = vsnprintf(buffer, bufferSize - 1, format, args);
			if ((result > 0) && (result < bufferSize)) {
				this->log(level, module, std::string(buffer));
				break;
			}

			bufferSize *= 4;
			if (bufferSize > 17000) { //Largest possible is 16384 chars.
				this->log(ofLogLevel::OF_LOG_ERROR, "CX_LoggerChannel", "Could not convert formatted arguments: "
						  "Resulting message would have been too long.");
				break;
			}
		}

		delete[] buffer;
	}


	///////////////////////
	// CX_LogMessageSink //
	///////////////////////
	CX_LogMessageSink::CX_LogMessageSink(void) :
		_logger(nullptr)
	{
	}
	
	CX_LogMessageSink::CX_LogMessageSink(CX_LogMessageSink&& ms)
	{
		*this = std::move(ms);
		ms._logger = nullptr;
	}

	CX_LogMessageSink::CX_LogMessageSink(CX::CX_Logger* logger, CX_LogLevel level, std::string module) :
		_logger(logger),
		_level(level),
		_module(std::move(module))
	{
		_message = std::shared_ptr<std::ostringstream>(new std::ostringstream);
	}

	CX_LogMessageSink::~CX_LogMessageSink(void) {
		if (_logger != nullptr) {
			_logger->_storeLogMessage(*this);
		}
	}

	CX_LogMessageSink& CX_LogMessageSink::operator << (std::ostream& (*func)(std::ostream&)) {
		func(*_message);
		return *this;
	}

} //namespace Private



CX_Logger::CX_Logger(void) :
	_flushCallback(nullptr),
	_logTimestamps(false),
	_timestampFormat("%H:%M:%S"),
	_defaultLogLevel(CX_LogLevel::LOG_NOTICE)
{
	levelForExceptions(CX_LogLevel::LOG_NONE);
	levelForConsole(CX_LogLevel::LOG_ALL);

	_ofLoggerChannel = ofPtr<CX::Private::CX_LoggerChannel>(new CX::Private::CX_LoggerChannel);

	ofAddListener(_ofLoggerChannel->messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	levelForAllModules(CX_LogLevel::LOG_ERROR);
}

CX_Logger::~CX_Logger(void) {
	this->captureOFLogMessages(false);

	ofRemoveListener(_ofLoggerChannel->messageLoggedEvent, this, &CX_Logger::_loggerChannelEventHandler);

	flush();

	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == CX::Private::LogTarget::FILE) {
			//_targetInfo[i].file->close(); //They should already be closed from flush()
			delete _targetInfo[i].file;
		}
	}
}

/*! Log all of the messages stored since the last call to flush() to the
selected logging targets. This is a blocking operation, because it may take
quite a while to output all log messages to various targets (see \ref blockingCode).
\note This function is not 100% thread-safe: Only call it from the main thread. */
void CX_Logger::flush(void) {

	//By getting the message count once and only iterating over that many messages,
	//a known number of messages are processed and just that many messages can be deleted later.
	_messageQueueMutex.lock();
	unsigned int messageCount = _messageQueue.size();
	_messageQueueMutex.unlock();

	if (messageCount == 0) {
		return;
	}

	//Open output files
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == CX::Private::LogTarget::FILE) {
			_targetInfo[i].file->open(_targetInfo[i].filename, ofFile::Append, false);
			if (!_targetInfo[i].file->is_open()) {
				std::cerr << "<CX_Logger> File " << _targetInfo[i].filename << " could not be opened for logging." << std::endl;
			}
		}
	}

	for (unsigned int i = 0; i < messageCount; i++) {
		//Lock and copy each message. Messages cannot be added while locked.
		//After the unlock, the message copy is used, not the original message.
		_messageQueueMutex.lock();
		CX::Private::CX_LogMessage m = _messageQueue[i];
		_messageQueueMutex.unlock();

		if (_flushCallback) {
			CX_MessageFlushData dat(m.message, m.level, m.module);
			_flushCallback(dat);
		}

		std::string formattedMessage = _formatMessage(m) + "\n";

		if (m.level >= _moduleLogLevels[m.module]) {
			for (unsigned int i = 0; i < _targetInfo.size(); i++) {
				if (m.level >= _targetInfo[i].level) {
					if (_targetInfo[i].targetType == CX::Private::LogTarget::CONSOLE) {
						cout << formattedMessage;
					} else if (_targetInfo[i].targetType == CX::Private::LogTarget::FILE) {
						*_targetInfo[i].file << formattedMessage;
					}
				}
			}
		}
	}

	//Close output files
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == CX::Private::LogTarget::FILE) {
			_targetInfo[i].file->close();
		}
	}

	//Delete printed messages
	_messageQueueMutex.lock();
	_messageQueue.erase(_messageQueue.begin(), _messageQueue.begin() + messageCount);
	_messageQueueMutex.unlock();
}

/*! \brief Clear all stored log messages. */
void CX_Logger::clear(void) {
	_messageQueueMutex.lock();
	_messageQueue.clear();
	_messageQueueMutex.unlock();
}

/*! \brief Set the log level for messages to be printed to the console. */
void CX_Logger::levelForConsole(CX_LogLevel level) {
	bool consoleFound = false;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if (_targetInfo[i].targetType == CX::Private::LogTarget::CONSOLE) {
			consoleFound = true;
			_targetInfo[i].level = level;
		}
	}

	if (!consoleFound) {
		CX::Private::CX_LoggerTargetInfo consoleTarget;
		consoleTarget.targetType = CX::Private::LogTarget::CONSOLE;
		consoleTarget.level = level;
		_targetInfo.push_back(consoleTarget);
	}
}

/*! Sets the log level for the file with the given file name. If the file does not exist, it will be created.
If the file does exist, it will be overwritten with a warning logged to cerr (typically the console).
\param level Log messages with level greater than or equal to this level will be outputted to the file. 
See the CX_LogLevel enum for valid values.
\param filename The name of the file to output to. If no file name is given, a file with name 
generated from a date/time from the start time of the experiment will be used.
*/
void CX_Logger::levelForFile(CX_LogLevel level, std::string filename) {
	if (filename == "CX_LOGGER_DEFAULT") {
		filename = "Log file " + CX::Instances::Clock.getExperimentStartDateTimeString("%Y-%b-%e %h-%M-%S %a") + ".txt";
	}
	filename = "logfiles/" + filename;
	filename = ofToDataPath(filename);

	bool fileFound = false;
	unsigned int fileIndex = -1;
	for (unsigned int i = 0; i < _targetInfo.size(); i++) {
		if ((_targetInfo[i].targetType == CX::Private::LogTarget::FILE) && (_targetInfo[i].filename == filename)) {
			fileFound = true;
			fileIndex = i;
			_targetInfo[i].level = level;
		}
	}

	//If nothing is to be logged, either delete or never create the target
	if (level == CX_LogLevel::LOG_NONE) {
		if (fileFound) {
			_targetInfo.erase(_targetInfo.begin() + fileIndex);
		}
		return;
	}

	if (!fileFound) {
		CX::Private::CX_LoggerTargetInfo fileTarget;
		fileTarget.targetType = CX::Private::LogTarget::FILE;
		fileTarget.level = level;
		fileTarget.filename = filename;
		fileTarget.file = new ofFile(); //This is deallocated in the dtor

		fileTarget.file->open(filename, ofFile::Reference, false);
		if (fileTarget.file->exists()) {
			std::cerr << "<CX_Logger> Log file already exists with name: " << filename << ". It will be overwritten." << std::endl;
		}

		fileTarget.file->open(filename, ofFile::WriteOnly, false);
		if (fileTarget.file->is_open()) {
			std::cout << "<CX_Logger> Log file \"" + filename + "\" opened." << std::endl;
		}
		*fileTarget.file << "CX log file. Created " << CX::Instances::Clock.getDateTimeString() << std::endl;
		fileTarget.file->close();

		_targetInfo.push_back(fileTarget);
	}
}

/*! Sets the log level for the given module. Messages from that module that are at a lower level than
\ref level will be ignored.
\param level See the CX::CX_LogLevel enum for valid values.
\param module A string representing one of the modules from which log messages are generated.
*/
void CX_Logger::level(CX_LogLevel level, std::string module) {
	_moduleLogLevelsMutex.lock();
	_moduleLogLevels[module] = level;
	_moduleLogLevelsMutex.unlock();
}

/*! Gets the log level in use by the given module.
\param module The name of the module.
\return The CX_LogLevel for `module`. */
CX_LogLevel CX_Logger::getModuleLevel(std::string module) {
	CX_LogLevel level = _defaultLogLevel;
	_moduleLogLevelsMutex.lock();
	if (_moduleLogLevels.find(module) != _moduleLogLevels.end()) {
		level = _moduleLogLevels[module];
	}
	_moduleLogLevelsMutex.unlock();
	return level;
}

/*!
Set the log level for all modules. This works both retroactively and proactively: All currently known modules
are given the log level and the default log level for new modules as set to the level.
*/
void CX_Logger::levelForAllModules(CX_LogLevel level) {
	_moduleLogLevelsMutex.lock();
	_defaultLogLevel = level;
	for (map<string, CX_LogLevel>::iterator it = _moduleLogLevels.begin(); it != _moduleLogLevels.end(); it++) {
		_moduleLogLevels[it->first] = level;
	}
	_moduleLogLevelsMutex.unlock();
}

/*!
Sets the user function that will be called on each message flush event. For every message that has been
logged, the user function will be called. No filtering is performed: All messages regardless of the module
log level will be sent to the user function.
\param f A pointer to a user function that takes a reference to a CX_MessageFlushData struct and returns nothing.
*/
void CX_Logger::setMessageFlushCallback(std::function<void(CX_MessageFlushData&)> f) {
	_flushCallback = f;
}

/*! Set whether or not to log timestamps and the format for the timestamps.
\param logTimestamps Does what it says.
\param format Timestamp format string. See http://pocoproject.org/docs/Poco.DateTimeFormatter.html#4684 for
documentation of the format. Defaults to %H:%M:%S.%i (24-hour clock with milliseconds at the end).
*/
void CX_Logger::timestamps(bool logTimestamps, std::string format) {
	_logTimestamps = logTimestamps;
	_timestampFormat = format;
}

/*! This is the fundamental logging function for this class. Example use:
\code{.cpp}
Log.log(CX_LogLevel::LOG_WARNING, "moduleName") << "Special message number: " << 20;
\endcode

Possible output: "[warning] <moduleName> Special message number: 20"

A newline is inserted automatically at the end of each message.

\param level Log level for this message. This has implications for message filtering. See CX::CX_Logger::level().
This should not be LOG_ALL or LOG_NONE, because that would be weird, wouldn't it?
\param module Name of the module that this log message is related to. This has implications for message filtering.
See CX::CX_Logger::level().
\return An object that can have log messages given to it as though it were a std::ostream.
\note This function and all of the trivial wrappers of this function (verbose(), notice(), warning(),
error(), fatalError()) are thread-safe.
*/
CX::Private::CX_LogMessageSink CX_Logger::log(CX_LogLevel level, std::string module) {
	return _log(level, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_VERBOSE, module)`. */
CX::Private::CX_LogMessageSink CX_Logger::verbose(std::string module) {
	return _log(CX_LogLevel::LOG_VERBOSE, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_NOTICE, module)`. */
CX::Private::CX_LogMessageSink CX_Logger::notice(std::string module) {
	return _log(CX_LogLevel::LOG_NOTICE, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_WARNING, module)`. */
CX::Private::CX_LogMessageSink CX_Logger::warning(std::string module) {
	return _log(CX_LogLevel::LOG_WARNING, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_ERROR, module)`. */
CX::Private::CX_LogMessageSink CX_Logger::error(std::string module) {
	return _log(CX_LogLevel::LOG_ERROR, module);
}

/*! \brief Equivalent to `log(CX_LogLevel::LOG_FATAL_ERROR, module)`. */
CX::Private::CX_LogMessageSink CX_Logger::fatalError(std::string module) {
	return _log(CX_LogLevel::LOG_FATAL_ERROR, module);
}

/*! Set this instance of CX_Logger to be the target of any messages created by openFrameworks logging functions.
This function is called during CX setup for CX::Instances::Log. You do not need to call it yourself. 

\param capture If `true`, capture oF log messages. If `false`, oF log messages go to the console.
*/
void CX_Logger::captureOFLogMessages(bool capture) {
	if (capture) {
		ofLogToConsole();
	} else {
		ofSetLoggerChannel(ofPtr<ofBaseLoggerChannel>(this->_ofLoggerChannel));
		ofSetLogLevel(ofLogLevel::OF_LOG_VERBOSE);
	}
}

/*! When a logged message is stored, if its log level is greater than or
equal to the exception level, an exception will be thrown. The exception
will be a std::runtime_error. By default, the exception level is LOG_NONE,
i.e. that no logged messages will cause an exception to be thrown.

\param level The desired exception level. The naming of the values in CX_LogLevel
is slightly confusing in the context of this function. Instead of, e.g., LOG_WARNING,
think of the value as EXCEPTION_ON_WARNING (or greater).
*/
void CX_Logger::levelForExceptions(CX_LogLevel level) {
	_exceptionLevel = level;
}

void CX_Logger::_storeLogMessage(const CX::Private::CX_LogMessageSink& ms) {
	//If the module is unknown to the logger, it becomes known with the default log level.
	_moduleLogLevelsMutex.lock();
	if (_moduleLogLevels.find(ms._module) == _moduleLogLevels.end()) {
		_moduleLogLevels[ms._module] = _defaultLogLevel;
	}
	_moduleLogLevelsMutex.unlock();


	CX::Private::CX_LogMessage temp(ms._level, ms._module);
	temp.message = (*ms._message).str();

	if (_logTimestamps) {
		temp.timestamp = CX::Instances::Clock.getDateTimeString(_timestampFormat);
	}

	_messageQueueMutex.lock();
	_messageQueue.push_back(temp);
	_messageQueueMutex.unlock();

	//Check for exceptions
	if (ms._level >= _exceptionLevel) {
		std::string formattedMessage = _formatMessage(temp);
		throw std::runtime_error(formattedMessage);
	}
}

/*
stringstream& CX_Logger::_log(CX_LogLevel level, std::string module) {
	
	_moduleLogLevelsMutex.lock();
	if (_moduleLogLevels.find(module) == _moduleLogLevels.end()) {
		_moduleLogLevels[module] = _defaultLogLevel;
	}
	_moduleLogLevelsMutex.unlock();

	CX::Private::CX_LogMessage temp(level, module);
	temp.message = std::shared_ptr<std::stringstream>(new std::stringstream);

	if (_logTimestamps) {
		temp.timestamp = CX::Instances::Clock.getDateTimeString(_timestampFormat);
	}

	_messageQueueMutex.lock();
	_messageQueue.push_back(temp);
	_messageQueueMutex.unlock();

	return *temp.message;
}
*/

CX::Private::CX_LogMessageSink CX_Logger::_log(CX_LogLevel level, std::string module) {
	return CX::Private::CX_LogMessageSink(this, level, module);
}

std::string CX_Logger::_getLogLevelString(CX_LogLevel level) {
	switch (level) {
	case CX_LogLevel::LOG_VERBOSE: return "verbose";
	case CX_LogLevel::LOG_NOTICE: return "notice";
	case CX_LogLevel::LOG_WARNING: return "warning";
	case CX_LogLevel::LOG_ERROR: return "error";
	case CX_LogLevel::LOG_FATAL_ERROR: return "fatal";
	case CX_LogLevel::LOG_ALL: return "all";
	case CX_LogLevel::LOG_NONE: return "none";
	};
	return "";
}

std::string CX_Logger::_formatMessage(const CX::Private::CX_LogMessage& message) {
	std::string logName = _getLogLevelString(message.level);
	logName.append(max<int>((int)(7 - logName.size()), 0), ' '); //Pad out names to 7 chars
	std::string formattedMessage;
	if (_logTimestamps) {
		formattedMessage += message.timestamp + " ";
	}

	formattedMessage += "[ " + logName + " ] ";

	if (message.module != "") {
		formattedMessage += "<" + message.module + "> ";
	}

	formattedMessage += message.message;

	return formattedMessage;
}

void CX_Logger::_loggerChannelEventHandler(CX::Private::CX_ofLogMessageEventData_t& md) {
	CX_LogLevel convertedLevel;

	switch (md.level) {
	case ofLogLevel::OF_LOG_VERBOSE: convertedLevel = CX_LogLevel::LOG_VERBOSE; break;
	case ofLogLevel::OF_LOG_NOTICE: convertedLevel = CX_LogLevel::LOG_NOTICE; break;
	case ofLogLevel::OF_LOG_WARNING: convertedLevel = CX_LogLevel::LOG_WARNING; break;
	case ofLogLevel::OF_LOG_ERROR: convertedLevel = CX_LogLevel::LOG_ERROR; break;
	case ofLogLevel::OF_LOG_FATAL_ERROR: convertedLevel = CX_LogLevel::LOG_FATAL_ERROR; break;
	case ofLogLevel::OF_LOG_SILENT: convertedLevel = CX_LogLevel::LOG_NONE; break;
	}

	this->_log(convertedLevel, md.module) << md.message;
}

} // namespace CX
