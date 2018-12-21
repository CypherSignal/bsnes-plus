//
// class and handler for debug events from external sources over stdin/stdout, e.g. VS Code
//
// This corresponds to the VSCode debug protocol outlined here: https://github.com/Microsoft/vscode/blob/master/src/vs/workbench/parts/debug/common/debugProtocol.d.ts
// Json objects - sepcifically, only Requests - are issued through stdin, 
// and corresponding Responses and Events are fed back through stdout for acknowledgement by the debugger
//

#include "extern-debug.moc"

#include <json/single_include/nlohmann/json.hpp>
#include <QTcpServer>

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

void writeJson(const nlohmann::json &jsonObj, QTcpSocket* socket)
{
  if (socket && socket->state() == QAbstractSocket::ConnectedState)
  {
    const auto& jsonDump = jsonObj.dump();
    string socketOutput;
    socketOutput = string() << "Content-Length: " << jsonDump.length() << "\r\n\r\n" << jsonDump.data() << "\n";
    socket->write(socketOutput());
  }
}

//////////////////////////////////////////////////////////////////////////

ExternDebugHandler::ExternDebugHandler(SymbolMap* symbolMap)
  : m_responseSeqId(0)
  , m_symbolMap(symbolMap)
  , m_debugProtocolServer(this)
  , m_debugProtocolConnection(nullptr)
{
  // Initialize the server so that it will listen for new connections
  int portAttempt = 5420;
  while (!m_debugProtocolServer.listen(QHostAddress::Any, portAttempt) && portAttempt < 5450)
  {
    ++portAttempt;
  }

  connect(&m_debugProtocolServer, &QTcpServer::newConnection,
    this, &ExternDebugHandler::openDebugProtocolConnection);
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

void ExternDebugHandler::stoppedEvent()
{
  nlohmann::json debugEvent = createEvent("stopped");
  debugEvent["body"]["reason"] = "pause"; 
  debugEvent["body"]["threadId"] = 0;
  debugEvent["body"]["allThreadsStopped"] = true;

  writeJson(debugEvent, m_debugProtocolConnection);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::continuedEvent()
{
  nlohmann::json debugEvent = createEvent("continued");
  debugEvent["body"]["threadId"] = 0;
  debugEvent["body"]["allThreadsContinued"] = true;

  writeJson(debugEvent, m_debugProtocolConnection);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::loadCartridgeEvent(const Cartridge& cartridge, const char* cartridgeFile)
{
  nlohmann::json debugEvent = createEvent("process");
  debugEvent["body"]["name"] = cartridgeFile;

  writeJson(debugEvent, m_debugProtocolConnection);

  debugEvent = createEvent("thread");
  debugEvent["body"]["reason"] = "started";
  debugEvent["body"]["threadId"] = 0;

  writeJson(debugEvent, m_debugProtocolConnection);

  // todo: based on cartridge.has_sa1, has_superfx, etc, add more threads.
  // Is there a deterministic threadId for each cpu core that can be used?
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::messageOutputEvent(const char* msg)
{
  nlohmann::json debugEvent = createEvent("output");
  debugEvent["body"]["output"] = msg;

  writeJson(debugEvent, m_debugProtocolConnection);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::openDebugProtocolConnection()
{
  QTcpSocket* debugProtocolConnection = m_debugProtocolServer.nextPendingConnection();

  m_debugProtocolConnection = debugProtocolConnection;

  // Handle disconnect event from the connection
  connect(debugProtocolConnection, &QAbstractSocket::disconnected,
    debugProtocolConnection, &QObject::deleteLater);

  connect(debugProtocolConnection, &QIODevice::readyRead,
    this, &handleDebugProtocolRead);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleDebugProtocolRead()
{
  QTcpSocket* debugConnection = m_debugProtocolConnection.data();
  if (!debugConnection)
    return;

  while (debugConnection->canReadLine())
  {
    char contentLengthStr[64];
    int contentLength = 0;
    debugConnection->readLine(contentLengthStr, 64);
    if (sscanf(contentLengthStr, "Content-Length: %d\r\n", &contentLength) != 1)
    {
      QMessageBox::information(nullptr, "Error reading content", "Error");
    }
    debugConnection->readLine();

    string contentString;
    contentLength += 1;
    contentString.reserve(contentLength);
    debugConnection->readLine(contentString(), contentLength);

    nlohmann::json jsonObj = nlohmann::json::parse(contentString(), contentString() + contentString.length(), nullptr, false);
    // todo: more robust handling required - need to check if the json obj is _compatible_
    if (!jsonObj.is_null())
    {
      this->handleRequest(jsonObj, debugConnection);
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleRequest(const nlohmann::json& request, QTcpSocket* responseConnection)
{
  nlohmann::json responseJson = createResponse(request);
  const auto& pendingReqStr = request["command"].get_ref<const nlohmann::json::string_t&>();
  // prepare any necessary update to the response
  switch (hashCalc(pendingReqStr.data(), pendingReqStr.size()))
  {
  case "retrorompreinit"_hash:
    responseJson["body"]["title"] = "bsnes-plus";
    responseJson["body"]["description"] = cartridge.fileName.length() ? (string() << "Running " << cartridge.fileName) : "No cartridge loaded";
    responseJson["body"]["pid"] = QCoreApplication::applicationPid();
    break;
  case "initialize"_hash:
    m_launchRequestReceived = false;
    m_preLaunchBreakpointRequests.reset();
    responseJson["body"]["supportsConfigurationDoneRequest"] = true;
    responseJson["body"]["supportsRestartRequest"] = true;
    responseJson["body"]["supportsTerminateRequest"] = true;
    writeJson(createEvent("initialized"), m_debugProtocolConnection);
    break;
  case "terminate"_hash:
    handleTerminateRequest(request);
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
    handleStackTraceRequest(request, responseJson);
    break;
  case "launch"_hash:
    handleLaunchRequest(request);
    handlePreLaunchBreakpoints(responseConnection);
    break;
  case "restart"_hash:
    handleRestartRequest(request);
    break;
  case "setBreakpoints"_hash:
    if (m_launchRequestReceived)
    {
      handleSetBreakpointRequest(request, responseJson);
    }
    else
    {
      m_preLaunchBreakpointRequests.append(request);
    }
    break;
  case "attach"_hash:
    handlePreLaunchBreakpoints(responseConnection);
    break;
  }
  writeJson(responseJson, responseConnection);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handlePreLaunchBreakpoints(QTcpSocket* responseConnection)
{
  m_launchRequestReceived = true;
  {
    for (int i = 0; i < m_preLaunchBreakpointRequests.size(); ++i)
    {
      nlohmann::json bpResponseJson = createResponse(m_preLaunchBreakpointRequests[i]);
      handleSetBreakpointRequest(m_preLaunchBreakpointRequests[i], bpResponseJson);
      writeJson(bpResponseJson, responseConnection);
    }
    m_preLaunchBreakpointRequests.reset();
  }
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleTerminateRequest(const nlohmann::json & pendingRequest)
{
  application.terminate = true;
  
  // the application responds to the 'terminate' setting very quickly,
  // so we need to flush the socket to definitely make sure the terminate event is sent
  writeJson(createEvent("terminated"), m_debugProtocolConnection);
  m_debugProtocolConnection->flush();
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleLaunchRequest(const nlohmann::json &pendingRequest)
{
  const nlohmann::json& launchArguments = pendingRequest["arguments"];

  // todo extract the fileBrowser::onAcceptCartridge logic to have the decision of
  // what kind of rom we're loading not be handled by UI, so that this can call that directly
  if (launchArguments["program"].is_string())
  {
    const auto& cartridgeFilename = launchArguments["program"].get_ref<const nlohmann::json::string_t&>();

    char cartridgeResolvedFilepath[PATH_MAX];
    ::realpath(cartridgeFilename.data(), cartridgeResolvedFilepath);
    messageOutputEvent(cartridgeResolvedFilepath);
    cartridge.loadNormal(cartridgeResolvedFilepath);
  }

  if (launchArguments["stopOnEntry"].is_boolean() && launchArguments["stopOnEntry"].get<bool>())
  {
    debugger->toggleRunStatus();
  }
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleRestartRequest(const nlohmann::json& pendingRequest)
{
  string prevCartridgeName = cartridge.baseName;
  cartridge.unload();
  cartridge.loadNormal(prevCartridgeName);
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleSetBreakpointRequest(const nlohmann::json& pendingRequest, nlohmann::json& responseJson)
{
  const nlohmann::json& sourceArg = pendingRequest["arguments"]["source"];
  
  uint32_t file = 0;
  uint32_t line = 0;

  if (sourceArg["path"].is_string())
  {
    const auto& sourcePath = sourceArg["path"].get_ref<const nlohmann::json::string_t&>();
    if (m_symbolMap && m_symbolMap->getFileIdFromPath(sourcePath.data(), file))
    {

      if (file >= m_activeBreakpoints.size())
      {
        m_activeBreakpoints.resize(file+1);
      }

      // first, clear all breakpoints associated with the given file
      for (int i = 0; i < m_activeBreakpoints[file].size(); ++i)
      {
        SNES::debugger.removeBreakpoint(m_activeBreakpoints[file][i]);
      }
      m_activeBreakpoints[file].reset();

      // then go through pendingRequest and identify all new breakpoints
      for (const auto& breakpoint : pendingRequest["arguments"]["breakpoints"])
      {
        line = breakpoint["line"];
        uint32_t discoveredLine = 0;
        uint32_t address = 0;
        bool foundLine = m_symbolMap->getSourceAddress(file, line, SymbolMap::AddressMatch_Closest, address, discoveredLine);

        char addrString[16];
        snprintf(addrString, 16, "%.8x", address);
        
        SNES::Debugger::Breakpoint bp;
        bp.addr = address;
        bp.enabled = true;
        m_activeBreakpoints[file].append(SNES::debugger.addBreakpoint(bp));

        responseJson["body"]["breakpoints"].push_back({
            {"verified", foundLine },
            {"line", discoveredLine},
            {"source", {"path", sourcePath} }
          });
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void ExternDebugHandler::handleStackTraceRequest(const nlohmann::json& pendingRequest, nlohmann::json& responseJson)
{
  // First, assemble a list of programAddresses and stackFrameAddresses
  nall::linear_vector<uint32_t> programAddresses;
  nall::linear_vector<uint32_t> stackFrameAddresses;

  unsigned stackFrameCount = SNES::cpu.stackFrames.count + 1; // 1 to allow for the pc
  programAddresses.reserve(stackFrameCount);
  stackFrameAddresses.reserve(stackFrameCount);
  
  programAddresses.append(SNES::cpu.regs.pc);
  stackFrameAddresses.append(0);

  if (SNES::cpu.stackFrames.count >= 1)
  {
    uint8_t prevBank = SNES::cpu.regs.pc.b;
    for (int framePtrIdx = SNES::cpu.stackFrames.count - 1; framePtrIdx >= 0; --framePtrIdx)
    {
      uint32_t framePtr = SNES::cpu.stackFrames.frameAddr[framePtrIdx];

      stackFrameAddresses.append(framePtr);

      uint32_t newAddr = 0;

      newAddr |= SNES::cpu.disassembler_read(++framePtr);
      newAddr |= SNES::cpu.disassembler_read(++framePtr) << 8;
      
      // if the framePtr is only 16-bits (jsr & rts) then we must be in the same bank, so implicitly use the last bank read
      bool extendedAddr = SNES::cpu.stackFrames.frameAddrIs24bit[framePtrIdx];
      uint8_t b = extendedAddr ? SNES::cpu.disassembler_read(++framePtr) : prevBank;
      prevBank = b;
      newAddr |= b << 16;

      programAddresses.append(newAddr);
    }
  }
  
  // Then fill out the responseJson with the addresses fetched above
  responseJson["body"]["totalFrames"] = stackFrameCount;
  for (unsigned i = 0; i < programAddresses.size(); ++i)
  {
    nlohmann::json& responseStackFrame = responseJson["body"]["stackFrames"][i];
    uint32_t pcAddr = programAddresses[i];
    uint32_t stackFrameAddr = (i <= stackFrameAddresses.size() ? stackFrameAddresses[i] : 0);
    uint32_t file = 0;
    uint32_t line = 0;

    string displayName;

    if (m_symbolMap && m_symbolMap->getSourceLineLocation(pcAddr, SymbolMap::AddressMatch_Closest, file, line))
    {
      responseStackFrame["line"] = line;

      if (const char* srcFilename = m_symbolMap->getSourceIncludeFilePath(file))
      {
        responseStackFrame["source"]["name"] = srcFilename;
      }

      if (const char* resolvedFilename = m_symbolMap->getSourceResolvedFilePath(file))
      {
        responseStackFrame["source"]["path"] = resolvedFilename;
        responseStackFrame["source"]["origin"] = "file";
      }

      string label;
      if (m_symbolMap->getLabel(pcAddr, SymbolMap::AddressMatch_Closest, label))
      {
        displayName = label << " - ";
      }
    }
    else
    {
      responseStackFrame["line"] = 0;
    }

    char pcAddrStr[64];
    if (stackFrameAddr != 0)
    {
      snprintf(pcAddrStr, 64, "0x%.6x (via 0x%.6x)", pcAddr, stackFrameAddr);
    }
    else
    {
      snprintf(pcAddrStr, 64, "0x%.6x", pcAddr);
    }

    displayName.append(pcAddrStr);

    responseStackFrame["name"] = displayName;
    responseStackFrame["id"] = 0;
    responseStackFrame["column"] = 0;

  }
}
