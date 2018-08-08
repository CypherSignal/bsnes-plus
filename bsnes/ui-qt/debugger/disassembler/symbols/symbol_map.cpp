#include "symbol_map.moc"
#include "symbol_file_adapters.cpp"

// ------------------------------------------------------------------------
Symbol Symbols::getSymbol() {
  for (uint32_t i=0; i<symbols.size(); i++) {
    if (symbols[i].isSymbol()) {
      return symbols[i];
    }
  }

  return Symbol::createInvalid();
}

  // ------------------------------------------------------------------------
Symbol Symbols::getComment() {
  for (uint32_t i=0; i<symbols.size(); i++) {
    if (symbols[i].isComment()) {
      return symbols[i];
    }
  }

  return Symbol::createInvalid();
}

// ------------------------------------------------------------------------
Symbol Symbols::getSourceLine()
{
  for (uint32_t i = 0; i < symbols.size(); i++) {
    if (symbols[i].isSourceLine()) {
      return symbols[i];
    }
  }

  return Symbol::createInvalid();
}


// ------------------------------------------------------------------------
SymbolMap::SymbolMap() {
  isValid = false;
  adapters = new SymbolFileAdapters();
}

// ------------------------------------------------------------------------
void SymbolMap::addLocation(uint32_t address, const string &name) {
  addSymbol(address, Symbol::createLocation(address, name));
}

// ------------------------------------------------------------------------
void SymbolMap::addComment(uint32_t address, const string &name) {
  addSymbol(address, Symbol::createComment(address, name));
}

// ------------------------------------------------------------------------
void SymbolMap::addSymbol(uint32_t address, const Symbol &name) {
  isValid = false;

  int32_t right = symbols.size();
  for (int32_t i=0; i<right; i++) {
    if (symbols[i].address == address) {
      symbols[i].symbols.append(Symbol(name));
      return;
    }
  }


  Symbols s;
  s.address = address;
  s.symbols.append(Symbol(name));
  symbols.append(s);
}

// ------------------------------------------------------------------------
void SymbolMap::addSourceLine(uint32_t address, uint32_t file, uint32_t line) {
  AddressToSourceLine newMapping;
  newMapping.address = address;
  newMapping.file = file;
  newMapping.line = line;
  addressToSourceLineMappings.append(newMapping);
}

// ------------------------------------------------------------------------
void SymbolMap::addSourceFile(uint32_t fileId, uint32_t checksum, const string &includeFilePath) {

  string sourceFileData, resolvedFilePath;
  if (fileId < sourceFileLines.size() && sourceFileLines[fileId].size() > 0) {
    debugger->echo(string() << "WARNING: While parsing symbols, file index " << fileId << " appeared for file \"" << sourceFiles[fileId].filename << "\" and \"" << includeFilePath << "\". Disassembly listing for either of these may be incorrect or unavailable.<br>");
  }
  else if (tryLoadSourceFile(includeFilePath, sourceFileData, resolvedFilePath)) {
    unsigned long local_checksum = crc32_calculate((const uint8_t*)(sourceFileData()), sourceFileData.length());
    if (checksum != local_checksum) {
      debugger->echo(string() << "WARNING: \"" << sourceFiles[fileId].filename << "\" has been modified since the ROM's symbols were built. Disassembly listing for this file may be incorrect or unavailable.<br>");
    }
    else {
      debugger->echo(string() << "Loaded source file " << includeFilePath << ".<br>");
      SourceFileInformation newFileInfo;
      newFileInfo.checksum = checksum;
      newFileInfo.filename = includeFilePath;
      newFileInfo.resolvedFilePath = resolvedFilePath;
      sourceFiles[fileId] = newFileInfo;
      sourceFileLines[fileId].split("\n", sourceFileData);
    }
  }
}

// ------------------------------------------------------------------------
bool SymbolMap::tryLoadSourceFile(const char* includeFilepath, string& sourceFileData, string& resolvedFilePath)
{
  char resolvedPathChrs[PATH_MAX];
  resolvedFilePath = string();
  if (sourceFileData.readfile(includeFilepath))
  {
    resolvedFilePath = ::realpath(includeFilepath, resolvedPathChrs);
    return true;
  }

  string tempResolvedFilePath;
  for (unsigned i = 0; i < sourceFilePaths.size(); ++i)
  {
    tempResolvedFilePath = string(sourceFilePaths[i], "/", includeFilepath);
    if (sourceFileData.readfile(tempResolvedFilePath))
    {
      resolvedFilePath = ::realpath(tempResolvedFilePath, resolvedPathChrs);
      return true;
    }
  }

  return false;
}

// ------------------------------------------------------------------------
void SymbolMap::finishUpdates() {
  // populate sourceline data, now that we have sourceline and sourcefile information
  // make sure symbols and addressToSourceLineMappings are sorted by addr
  if (addressToSourceLineMappings.size() > 0) {
    revalidate();
    nall::sort(&addressToSourceLineMappings[0], addressToSourceLineMappings.size());


    uint32_t symbolIndex = 0;
    uint32_t symbolIndexEnd = symbols.size();
    string sourceLine;
    for (int i = 0; i < addressToSourceLineMappings.size(); ++i) {
      AddressToSourceLine addrToLine = addressToSourceLineMappings[i];

      const char* sourceLineRaw = getSourceLineFromLocation(addrToLine.file, addrToLine.line);
      if (sourceLineRaw == nullptr) {
        // SYMBOLS-TODO this might be crashing?
        //char hexAddr[9];
        //snprintf(hexAddr, 9, "%.8x", addrToLine.address);
        //debugger->echo(string() << "WARNING: Address-to-line mapping for address 0x" << hexAddr << " tried to refer to a file/line location that doesn't exist. File: " << addrToLine.file << ", line: " << addrToLine.line << ".<br>");
        continue;
      }

      // advance symbolindex forward until its address is >= the current addrToLine address
      while (symbols[symbolIndex].address < addrToLine.address && symbolIndex < symbolIndexEnd) {
        ++symbolIndex;
      }

      sourceLine = sourceLineRaw;
      // create new symbol if we didn't match the address
      if (symbols[symbolIndex].address > addrToLine.address) {
        Symbols s;
        s.address = addrToLine.address;
        s.symbols.append(Symbol::createSourceLine(addrToLine.address, sourceLine));
        symbols.append(s);
      }
      else {
        symbols[symbolIndex].symbols.append(Symbol::createSourceLine(addrToLine.address, sourceLine));
      }
    }
  }
  emit updated();
}

// ------------------------------------------------------------------------
void SymbolMap::revalidate() {
  if (isValid) {
    return;
  }

  // Don't know how to do this with pure nall stuff :(
  int numSymbols = symbols.size();
  Symbols *temp = new Symbols[numSymbols];
  for (int i=0; i<numSymbols; i++) {
    temp[i] = symbols[i];
  }

  nall::sort(temp, numSymbols);

  symbols.reset();
  symbols.reserve(numSymbols);
  for (int i=0; i<numSymbols; i++) {
    symbols.append(temp[i]);
  }

  isValid = true;
}

// ------------------------------------------------------------------------
Symbol SymbolMap::getSymbol(uint32_t address) {
  int32_t index = getSymbolIndex(address);
  if (index == -1) {
    return Symbol::createInvalid();
  }

  return symbols[index].getSymbol();
}

// ------------------------------------------------------------------------
Symbol SymbolMap::getComment(uint32_t address) {
  int32_t index = getSymbolIndex(address);
  if (index == -1) {
    return Symbol::createInvalid();
  }

  return symbols[index].getComment();
}

// ------------------------------------------------------------------------
Symbol SymbolMap::getSourceLine(uint32_t address) {
  int32_t index = getSymbolIndex(address);
  if (index == -1) {
    return Symbol::createInvalid();
  }

  return symbols[index].getSourceLine();
}

// ------------------------------------------------------------------------
int32_t SymbolMap::getSymbolIndex(uint32_t address) {
  revalidate();

  int32_t left = 0;
  int32_t right = symbols.size() - 1;

  while (right >= left) {
    uint32_t cur = ((right - left) >> 1) + left;
    uint32_t curaddr = symbols[cur].address;

    if (address < curaddr) {
      right = cur - 1;
    }
    else if (address > curaddr) {
      left = cur + 1;
    }
    else {
      return cur;
    }
  }

  return -1;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceLineLocation(uint32_t address, uint32_t& outFile, uint32_t &outLine)
{
  revalidate();

  int32_t left = 0;
  int32_t right = addressToSourceLineMappings.size() - 1;

  while (right >= left) {
    uint32_t cur = ((right - left) >> 1) + left;
    uint32_t curaddr = addressToSourceLineMappings[cur].address;

    if (address < curaddr) {
      right = cur - 1;
    }
    else if (address > curaddr) {
      left = cur + 1;
    }
    else {
      outFile = addressToSourceLineMappings[cur].file;
      outLine = addressToSourceLineMappings[cur].line;
      return true;
    }
  }

  return false;
}

// ------------------------------------------------------------------------
const char* SymbolMap::getSourceLineFromLocation(uint32_t file, uint32_t line)
{
  if (file < sourceFileLines.size() && line < sourceFileLines[file].size()) {
    return (const char*)sourceFileLines[file][line - 1](); // -1 because line entries are 1-based
  }
  else {
    return nullptr;
  }
}

// ------------------------------------------------------------------------
const char* SymbolMap::getSourceIncludeFilePath(uint32_t file)
{
  if (file < sourceFiles.size()) {
    return (const char*)sourceFiles[file].filename();
  }
  else {
    return nullptr;
  }
}

// ------------------------------------------------------------------------
const char* SymbolMap::getSourceResolvedFilePath(uint32_t file)
{
  if (file < sourceFiles.size()) {
    return (const char*)sourceFiles[file].resolvedFilePath();
  }
  else {
    return nullptr;
  }
}

// ------------------------------------------------------------------------
bool SymbolMap::getFileIdFromPath(const char* resolvedFilePath, uint32_t& outFile)
{
  for (unsigned i = 0; i < sourceFiles.size(); ++i)
  {
    if (sourceFiles[i].resolvedFilePath == resolvedFilePath)
    {
      outFile = i;
      return true;
    }
  }
  return false;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceAddress(uint32_t file, uint32_t line, uint32_t& outAddress, uint32_t& outLine)
{
  for (unsigned i = 0; i < addressToSourceLineMappings.size(); ++i)
  {
    if (addressToSourceLineMappings[i].file == file && addressToSourceLineMappings[i].line >= line)
    {
      outAddress = addressToSourceLineMappings[i].address;
      outLine = addressToSourceLineMappings[i].line;
      return true;
    }
  }
  return false;
}

// ------------------------------------------------------------------------
void SymbolMap::removeSymbol(uint32_t address, Symbol::Type type) {
  int32_t index = getSymbolIndex(address);
  if (index == -1) {
    return;
  }

  Symbols &s = symbols[index];
  for (int32_t i=0; i<s.symbols.size(); i++) {
    if (s.symbols[i].type == type) {
      s.symbols.remove(i);
      i--;
    }
  }

  if (s.symbols.size() == 0) {
    symbols.remove(index);
    isValid = false;
  }
}

// ------------------------------------------------------------------------
void SymbolMap::loadFromFile(const string &baseName, const string &ext) {
  string fileName = baseName;
  fileName.append(ext);

  ::nall::file f;
  if (!f.open((const char*)fileName, ::nall::file::mode::read)) {
    return;
  }

  debugger->echo(string() << "Loading symbols from " << fileName << ".<br>");

  sourceFilePaths.reset();
  sourceFilePaths.append(nall::dir(fileName));

  int size = f.size();
  char *buffer = new char[size + 1];
  buffer[size] = 0;
  f.read((uint8_t*)buffer, f.size());
  loadFromString(buffer);

  delete[] buffer;

  f.close();
}

// ------------------------------------------------------------------------
void SymbolMap::loadFromString(const string &file) {
  nall::lstring rows;
  rows.split("\n", file);

  SymbolFileInterface *adapter = adapters->findBestAdapter(rows);
  if (adapter == NULL) {
    return;
  }

  if (adapter->read(rows, this)) {
    finishUpdates();
  }
}

// ------------------------------------------------------------------------
void SymbolMap::unloadAll()
{
  symbols.reset();
  addressToSourceLineMappings.reset();
  sourceFiles.reset();
  sourceFileLines.reset();
}

// ------------------------------------------------------------------------
