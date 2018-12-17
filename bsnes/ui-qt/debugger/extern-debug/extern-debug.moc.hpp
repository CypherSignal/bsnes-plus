//
// class and handler for debug events from external sources over stdin/stdout, e.g. VS Code
//

#ifndef __EXTERN_DEBUG__H__
#define __EXTERN_DEBUG__H__

// Note that usage of the nlohmann/json git repo can be removed once Qt5 is in
// (see also Makefile)
#include <json/single_include/nlohmann/json.hpp>

#include <QTcpServer>
#include <QTcpSocket>

//////////////////////////////////////////////////////////////////////////

class ExternDebugHandler : public QObject {
  Q_OBJECT

public:
  ExternDebugHandler(SymbolMap* symbolMap);

  void stoppedEvent();
  void continuedEvent();
  void loadCartridgeEvent(const Cartridge& cartridge, const char* cartridgeFile);
  void messageOutputEvent(const char* msg);
public slots:
  void openDebugProtocolConnection();
  void handleDebugProtocolRead();

private:
  void handleRequest(const nlohmann::json& request, QTcpSocket* responseConnection);
  
  void handleTerminateRequest(const nlohmann::json& pendingRequest);
  void handleLaunchRequest(const nlohmann::json& pendingRequest);
  void handleRestartRequest(const nlohmann::json& pendingRequest);
  void handleSetBreakpointRequest(nlohmann::json& responseJson, const nlohmann::json& pendingRequest);
  void handleStackTraceRequest(nlohmann::json& responseJson, const nlohmann::json& pendingRequest);

  nlohmann::json createResponse(const nlohmann::json& request);
  nlohmann::json createEvent(const char* eventType);

  SymbolMap *m_symbolMap;

  int m_responseSeqId;

  QTcpServer m_debugProtocolServer;
  QPointer<QTcpSocket> m_debugProtocolConnection;

};

extern ExternDebugHandler* externDebugHandler;

#endif // __EXTERN_DEBUG__H__