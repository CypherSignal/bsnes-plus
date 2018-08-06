//
// class and handler for debug events from external sources over stdin/stdout, e.g. VS Code
//

#ifndef __EXTERN_DEBUG__H__
#define __EXTERN_DEBUG__H__

// Note that usage of the nlohmann/json git repo can be removed once Qt5 is in
// (see also Makefile)
#include <json/single_include/nlohmann/json.hpp>

//////////////////////////////////////////////////////////////////////////

class RequestListenerThread : public QThread
{
  Q_OBJECT
public:
  using QThread::QThread;

  void run();

  QMutex* requestQueueMutex;
  QQueue<nlohmann::json>* requestQueue;

private:
  FILE * m_stdinLog;
};

//////////////////////////////////////////////////////////////////////////

class ExternDebugHandler : public QObject {
  Q_OBJECT

public:
  ExternDebugHandler();
  void processRequests();

  void stoppedEvent();
  void continuedEvent();
  void loadCartridgeEvent(const char* cartridgeFile);

public slots:

private:
  nlohmann::json createResponse(const nlohmann::json& request);
  nlohmann::json createEvent(const char* eventType);

  RequestListenerThread* m_requestListenerThread;
  QMutex m_requestQueueMutex;
  QQueue<nlohmann::json> m_requestQueue;

  QQueue<nlohmann::json> m_eventQueue;

  int m_responseSeqId;
  FILE* m_stdoutLog;
};

extern ExternDebugHandler* externDebugHandler;

#endif // __EXTERN_DEBUG__H__