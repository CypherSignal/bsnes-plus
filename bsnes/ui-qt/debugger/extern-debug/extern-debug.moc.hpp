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
  void handlePreLaunchBreakpoints(QTcpSocket* responseConnection);

  void handleTerminateRequest(const nlohmann::json& pendingRequest);
  void handleLaunchRequest(const nlohmann::json& pendingRequest);
  void handleRestartRequest(const nlohmann::json& pendingRequest);
  void handleSetBreakpointRequest(const nlohmann::json& pendingRequest, nlohmann::json& responseJson);
  void handleStackTraceRequest(const nlohmann::json& pendingRequest, nlohmann::json& responseJson);

  nlohmann::json createResponse(const nlohmann::json& request);
  nlohmann::json createEvent(const char* eventType);

  SymbolMap *m_symbolMap;

  int m_responseSeqId;

  QTcpServer m_debugProtocolServer;
  QPointer<QTcpSocket> m_debugProtocolConnection;

  // Before launch request is sent, some breakpoint requests are sent.
  // Keep those off to the side until launch is done, and then fire them immediately
  bool m_launchRequestReceived;
  nall::linear_vector<nlohmann::json> m_preLaunchBreakpointRequests;

  // the SetBreakpoint request specification is such that a bundle of breakpoints for
  // one file are all sent at once, and all previous breakpoints from that file should
  // be removed. This is a vector that stores for each fileId, a vector of active breakpoints.
  nall::linear_vector<nall::linear_vector<int>> m_activeBreakpoints;
};

extern ExternDebugHandler* externDebugHandler;

#endif // __EXTERN_DEBUG__H__