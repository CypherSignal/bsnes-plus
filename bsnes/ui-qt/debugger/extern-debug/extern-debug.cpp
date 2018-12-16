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
#include <stdio.h>
#include <sys/stat.h>

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

void writeJson(const nlohmann::json &jsonObj, QTcpSocket& socket)
{
  const auto& jsonDump = jsonObj.dump();
  if (socket.state() == QAbstractSocket::ConnectedState)
  {
    string socketOutput;
    socketOutput = string() << "Content-Length: " << jsonDump.length() << "\r\n\r\n" << jsonDump.data() << "\n";
    socket.write(socketOutput());
  }
}

//////////////////////////////////////////////////////////////////////////

ExternDebugHandler::ExternDebugHandler(SymbolMap* symbolMap)
  : m_responseSeqId(0)
  , m_symbolMap(symbolMap)
  , m_debugProtocolServer(this)
  , m_debugProtocolConnection(nullptr)
{
  {
    int attemptCounter = 0;
    while (!m_debugProtocolServer.listen(QHostAddress::Any, 5422 + attemptCounter) && attemptCounter < 100)
    {
      ++attemptCounter;
    }
  }
  connect(&m_debugProtocolServer, &QTcpServer::newConnection,
    this, &ExternDebugHandler::openDebugProtocolConnection);
}

//////////////////////////////////////////////////////////////////////////
// dcrooks-todo handle "Close"/Terminate/disconnect request
void ExternDebugHandler::processRequests()
{
  while (!m_requestQueue.empty())
  {
    nlohmann::json pendingRequest = m_requestQueue.dequeue();
    nlohmann::json responseJson = createResponse(pendingRequest);

    const auto& pendingReqStr = pendingRequest["command"].get_ref<const nlohmann::json::string_t&>();

    // prepare any necessary update to the response
    switch (hashCalc(pendingReqStr.data(), pendingReqStr.size()))
    {
    case "retrorompreinit"_hash:
      responseJson["body"]["title"] = "bsnes-plus";
      responseJson["body"]["description"] = cartridge.fileName.length() ? (string() << "Running " << cartridge.fileName) : "No cartridge loaded";
      responseJson["body"]["pid"] = QCoreApplication::applicationPid();
      break;
    case "initialize"_hash:
      responseJson["body"]["supportsConfigurationDoneRequest"] = true;
      responseJson["body"]["supportsRestartRequest"] = true;
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
    case "launch"_hash:
      handleLaunchRequest(pendingRequest);
      break;
    case "restart"_hash:
      handleRestartRequest(pendingRequest);
    break;
    case "setBreakpoints"_hash:
      handleSetBreakpointRequest(responseJson, pendingRequest);
      break;
    }
    writeJson(responseJson, *m_debugProtocolConnection);
  }

  while (!m_eventQueue.empty())
  {
    if (m_debugProtocolConnection && m_debugProtocolConnection->state() == QAbstractSocket::ConnectedState)
    {
      writeJson(m_eventQueue.dequeue(), *m_debugProtocolConnection);
    }
    else
    {
      m_eventQueue.dequeue();
    }
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

void ExternDebugHandler::handleSetBreakpointRequest(nlohmann::json& responseJson, const nlohmann::json& pendingRequest)
{
  const nlohmann::json& sourceArg = pendingRequest["arguments"]["source"];
  
  uint32_t file = 0;
  uint32_t line = 0;

  // dcrooks-todo: this wipes out all breakpoints across all files, right?
  breakpointEditor->clear();

  if (sourceArg["path"].is_string())
  {
    const auto& sourcePath = sourceArg["path"].get_ref<const nlohmann::json::string_t&>();
    if (m_symbolMap && m_symbolMap->getFileIdFromPath(sourcePath.data(), file))
    {
      for (const auto& breakpoint : pendingRequest["arguments"]["breakpoints"])
      {
        line = breakpoint["line"];
        uint32_t discoveredLine = 0;
        uint32_t address = 0;
        bool foundLine = m_symbolMap->getSourceAddress(file, line, SymbolMap::AddressMatch_Closest, address, discoveredLine);

        char addrString[16];
        snprintf(addrString, 16, "%.8x", address);
        breakpointEditor->addBreakpoint(addrString, "x", "cpu");

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

void ExternDebugHandler::handleStackTraceRequest(nlohmann::json& responseJson, const nlohmann::json& pendingRequest)
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

//////////////////////////////////////////////////////////////////////////

// dcrooks-todo need to look for more improvements to this whole setup.
// may not need a requestQueue or eventQueue, for example. Can we send json objs over Qt Signals?
void ExternDebugHandler::openDebugProtocolConnection()
{
  m_debugProtocolConnection = m_debugProtocolServer.nextPendingConnection();
  connect(m_debugProtocolConnection, &QAbstractSocket::disconnected,
    m_debugProtocolConnection, &QObject::deleteLater);

  connect(m_debugProtocolConnection, &QIODevice::readyRead,
    this, &ExternDebugHandler::handleSocketRead);
}

void ExternDebugHandler::handleSocketRead()
{
  string contentString;
  contentString.reserve(1024);

  char contentLengthStr[64];
  int contentLength = 0;
  while (m_debugProtocolConnection->canReadLine())
  {
    m_debugProtocolConnection->readLine(contentLengthStr, 64);
    if (sscanf(contentLengthStr, "Content-Length: %d\r\n", &contentLength) != 1)
    {
      QMessageBox::information(nullptr, "Error reading content", "Error");
    }

    m_debugProtocolConnection->readLine();

    contentLength += 1;
    contentString.reserve(contentLength);
    m_debugProtocolConnection->readLine(contentString(), contentLength);

    nlohmann::json jsonObj = nlohmann::json::parse(contentString(), contentString() + contentString.length(), nullptr, false);
    // todo: more robust handling required - need to check if the json obj is _compatible_
    if (!jsonObj.is_null())
    {
      m_requestQueue.enqueue(jsonObj);
    }
  }
}
