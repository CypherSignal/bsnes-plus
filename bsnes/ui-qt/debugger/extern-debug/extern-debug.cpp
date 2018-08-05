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

// todo: should be able to make this atomic
void ConcurrentJsonQueue::enqueue(const nlohmann::json& t)
{
  QMutexLocker _(&m_mutex);
  m_queue.enqueue(t);
}

nlohmann::json ConcurrentJsonQueue::dequeue()
{
  QMutexLocker _(&m_mutex);
  return m_queue.dequeue();
}

bool ConcurrentJsonQueue::empty()
{
  QMutexLocker _(&m_mutex);
  return m_queue.empty();
}

//////////////////////////////////////////////////////////////////////////

// function that runs in its own thread to monitor stdin for requests, then push them onto the provided queue
void RequestListenerThread::run()
{
  // todo: make the log optional
  m_stdinLog = fopen("requestLog.txt", "wb");

  // check if stdin is set up for reading (e.g. we only launched a window without any attachment)
  // TODO: how can vscode be set up for a delayed attach to bsnes? what comms happens over stdin at that point?
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
      requestQueue->enqueue(jsonObj);
    }
  }
  fclose(m_stdinLog);
}

//////////////////////////////////////////////////////////////////////////

void writeJson(nlohmann::json &jsonObj, FILE* file)
{
  auto jsonDump = jsonObj.dump();
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
  while (!m_requestQueue.empty())
  { 
    nlohmann::json pendingRequest = m_requestQueue.dequeue();
    
    nlohmann::json responseJson;
    responseJson["seq"] = m_responseSeqId++;
    responseJson["type"] = "response";
    responseJson["request_seq"] = pendingRequest["seq"];
    responseJson["command"] = pendingRequest["command"];
    responseJson["success"] = true;
    writeJson(responseJson, m_stdoutLog);

    nlohmann::json outputEventJson;
    outputEventJson["seq"] = m_responseSeqId++;
    outputEventJson["type"] = "event";
    outputEventJson["event"] = "output";
    outputEventJson["body"]["output"] = "This is a test.\n";
    writeJson(outputEventJson, m_stdoutLog);
  }

  // todo : issue events here, too?
}

//////////////////////////////////////////////////////////////////////////

