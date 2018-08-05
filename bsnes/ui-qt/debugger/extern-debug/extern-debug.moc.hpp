//
// class and handler for debug events from external sources over stdin/stdout, e.g. VS Code
//

#ifndef __EXTERN_DEBUG__H__
#define __EXTERN_DEBUG__H__

// Note that usage of the nlohmann/json git repo can be removed once Qt5 is in
// (see also Makefile)
#include <json/single_include/nlohmann/json.hpp>

class ConcurrentJsonQueue
{
public:
  nlohmann::json dequeue();
  void enqueue(const nlohmann::json& t);
  bool empty();
private:
  QMutex m_mutex;
  QQueue<nlohmann::json> m_queue;
};

class RequestListenerThread : public QThread
{
  Q_OBJECT
public:
  using QThread::QThread;

  void run();

  ConcurrentJsonQueue* requestQueue;

private:
  FILE * m_stdinLog;
};

//////////////////////////////////////////////////////////////////////////

class ExternDebugHandler : public QObject {
  Q_OBJECT

public:
  ExternDebugHandler();
  void processRequests();

public slots:

private:
  RequestListenerThread* m_requestListenerThread;
  
  ConcurrentJsonQueue m_requestQueue;

  FILE* m_stdoutLog;

  int m_responseSeqId;
};

extern ExternDebugHandler* externDebugHandler;

#endif // __EXTERN_DEBUG__H__