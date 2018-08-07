//
// class and handler for debug events from external sources over stdin/stdout, e.g. VS Code
//
// This corresponds to the VSCode debug protocol outlined here: https://github.com/Microsoft/vscode/blob/master/src/vs/workbench/parts/debug/common/debugProtocol.d.ts
// Json objects - sepcifically, only Requests - are issued through stdin, 
// and corresponding Responses and Events are fed back through stdout for acknowledgement by the debugger
//

#include "extern-debug.moc"

#include <json/single_include/nlohmann/json.hpp>
#include <stdio.h>
#include <sys/stat.h>

#ifdef WIN32  
#include <fcntl.h>  
#include <io.h>  
#endif

ExternDebugHandler *externDebugHandler;

//////////////////////////////////////////////////////////////////////////
// simple string-hash function for switching on response types

constexpr unsigned long hashCalc(const char* ch, size_t len)
{
  return len > 0 ? ((unsigned long)(*ch) + hashCalc(ch + 1, len - 1) * 33) % (1 << 26) : 0;
}
constexpr unsigned long operator "" _hash(const char* ch, size_t len)
{
  return hashCalc(ch, len);
}

//////////////////////////////////////////////////////////////////////////

// function that runs in its own thread to monitor stdin for requests, then push them onto the provided queue
void RequestListenerThread::run()
{
  // todo: make the log optional
  m_stdinLog = fopen("requestLog.txt", "wb");

  // check if stdin is set up for reading (e.g. we only launched a window without any attachment)
  // TODO: how can vscode be set up for a delayed attach to bsnes? what comms happens over stdin in that case?
  {
    fprintf(m_stdinLog, "Checking stdin fstat\n");
    struct stat stdinStat;
    if (fstat(fileno(stdin), &stdinStat) != 0)
    {
      fclose(m_stdinLog);
      return;
    }
  }
  fprintf(m_stdinLog, "Entering stdin loop\n");

  const int MaxWarnings = 50;
  int warnings = 0;

  string contentString;
  contentString.reserve(1024);

  char contentLengthStr[64];
  int contentLength = 0;
  while (!feof(stdin))
  {
    fprintf(m_stdinLog, "Reading input\n");
    fflush(m_stdinLog);

    // fetch a line from stdin and scan it for content header
    fgets(contentLengthStr, 64, stdin);
    // todo: error handling for when the scan fails
    if (sscanf(contentLengthStr, "Content-Length: %d\r\n", &contentLength) != 1)
    {
      fprintf(m_stdinLog, "WARNING: While searching for Content-Length string, received found string of length %d: %s", strlen(contentLengthStr), contentLengthStr);
      if (++warnings < MaxWarnings)
        continue;
      else
        break;
    }
    // capture the additional /r/n from the content header
    fgets(contentLengthStr, 64, stdin);

    fprintf(m_stdinLog, "Reading content-length: %d\n", contentLength);

    contentLength += 1; // add 1 to allow for null-terminator in fgets
    contentString.reserve(contentLength);
    fgets(contentString(), contentLength, stdin);

    // mirror the input to a file
    fprintf(m_stdinLog, "%s\r\n", contentString());

    nlohmann::json jsonObj = nlohmann::json::parse(contentString(), contentString() + contentString.length(), nullptr, false);
    // todo: more robust handling required - need to check if the json obj is _compatible_
    if (!jsonObj.is_null())
    {
      QMutexLocker _(requestQueueMutex);
      requestQueue->enqueue(jsonObj);
    }
  }
  fclose(m_stdinLog);
}

//////////////////////////////////////////////////////////////////////////

void writeJson(const nlohmann::json &jsonObj, FILE* file)
{
  const auto& jsonDump = jsonObj.dump();
  if (file)
  {
    fprintf(file, "Content-Length: %d\r\n\r\n%s\n", jsonDump.length(), jsonDump.data());
    fflush(file);
  }
  fprintf(stdout, "Content-Length: %d\r\n\r\n%s", jsonDump.length(), jsonDump.data());
  fflush(stdout);
}

//////////////////////////////////////////////////////////////////////////

ExternDebugHandler::ExternDebugHandler()
  :m_responseSeqId(0)
{
  m_requestListenerThread = new RequestListenerThread(this);
  m_requestListenerThread->requestQueueMutex = &m_requestQueueMutex;
  m_requestListenerThread->requestQueue = &m_requestQueue;

  m_requestListenerThread->start();

  m_stdoutLog = fopen("responseEventLog.txt", "wb");

#ifdef PLATFORM_WIN
  // required so that we don't inject additional CR's into stdout
  //todo: do we need this? we're compiling in mingw, which may have slightly diff behaviour for stdout by default...
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::processRequests()
{
  {
    QMutexLocker _(&m_requestQueueMutex);
    while (!m_requestQueue.empty())
    {
      nlohmann::json pendingRequest = m_requestQueue.dequeue();
      nlohmann::json responseJson = createResponse(pendingRequest);

      const auto& pendingReqStr = pendingRequest["command"].get_ref<const nlohmann::json::string_t&>();

      // prepare any necessary update to the response
      switch (hashCalc(pendingReqStr.data(), pendingReqStr.size()))
      {
      case "initialize"_hash:
        responseJson["body"]["supportsConfigurationDoneRequest"] = true;
        m_eventQueue.enqueue(createEvent("initialized"));
        break;
      case "pause"_hash:
      case "continue"_hash:
        debugger->toggleRunStatus();
        break;
      case "stepIn"_hash:
        debugger->stepAction();
        break;
      case "next"_hash:
        debugger->stepOverAction();
        break;
      case "stepOut"_hash:
        debugger->stepOutAction();
        break;
      case "threads"_hash:
        responseJson["body"]["threads"][0]["name"] = "CPU";
        responseJson["body"]["threads"][0]["id"] = 0;
        break;
      case "stackTrace"_hash:
        handleStackTraceRequest(responseJson, pendingRequest);
        break;
      }
      writeJson(responseJson, m_stdoutLog);
    }
  }

  while (!m_eventQueue.empty())
  {
    writeJson(m_eventQueue.dequeue(), m_stdoutLog);
  }
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::stoppedEvent()
{
  nlohmann::json debugEvent = createEvent("stopped");
  debugEvent["body"]["reason"] = "pause"; 
  debugEvent["body"]["threadId"] = 0;
  debugEvent["body"]["allThreadsStopped"] = true;

  m_eventQueue.enqueue(debugEvent);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::continuedEvent()
{
  nlohmann::json debugEvent = createEvent("continued");
  debugEvent["body"]["threadId"] = 0;
  debugEvent["body"]["allThreadsContinued"] = true;

  m_eventQueue.enqueue(debugEvent);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::loadCartridgeEvent(const Cartridge& cartridge, const char* cartridgeFile)
{
  nlohmann::json debugEvent = createEvent("process");
  debugEvent["body"]["name"] = cartridgeFile;
  m_eventQueue.enqueue(debugEvent);

  debugEvent = createEvent("thread");
  debugEvent["body"]["reason"] = "started";
  debugEvent["body"]["threadId"] = 0;
  m_eventQueue.enqueue(debugEvent);

  // todo: based on cartridge.has_sa1, has_superfx, etc, add more threads.
  // Is there a deterministic threadId for each cpu core that can be used?
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleStackTraceRequest(nlohmann::json& responseJson, const nlohmann::json& pendingRequest)
{
  unsigned stackFrameCount = 1; // 1 to allow for the pc 
  responseJson["body"]["totalFrames"] = stackFrameCount;

  char stackName[32];

  snprintf(stackName, 32, "0x%.6x", SNES::cpu.regs.pc);

  responseJson["body"]["stackFrames"][0]["id"] = 0;
  responseJson["body"]["stackFrames"][0]["name"] = stackName;
  responseJson["body"]["stackFrames"][0]["line"] = 0;
  responseJson["body"]["stackFrames"][0]["column"] = 0;
}

//////////////////////////////////////////////////////////////////////////

nlohmann::json ExternDebugHandler::createResponse(const nlohmann::json& request)
{
  return nlohmann::json::object({
    { "seq", m_responseSeqId++ },
    { "type", "response" },
    { "request_seq", request["seq"] },
    { "command", request["command"] },
    { "success", true } // response handlers should flip this to false only if needed
  });
}

//////////////////////////////////////////////////////////////////////////

nlohmann::json ExternDebugHandler::createEvent(const char* eventType)
{
  return nlohmann::json::object({
    { "seq", m_responseSeqId++},
    { "type", "event" },
    { "event", eventType }
    });
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::messageOutputEvent(const char* msg)
{
  nlohmann::json debugEvent = createEvent("output");
  debugEvent["body"]["output"] = msg;
  m_eventQueue.enqueue(debugEvent);
}