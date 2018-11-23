#include "symbol_map.moc"
#include "symbol_file_adapters.cpp"

// ------------------------------------------------------------------------
SymbolMap::SymbolMap() {
  isValid = false;
  adapters = new SymbolFileAdapters();
}

// ------------------------------------------------------------------------
void SymbolMap::addLabel(uint32_t address, const string &name) {
  isValid = false;
  labels.append({ address, name });
}

// ------------------------------------------------------------------------
void SymbolMap::addComment(uint32_t address, const string &name) {
  isValid = false;
  comments.append({ address, name });
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
  if (addressToSourceLineMappings.size() > 0) {
    isValid = false;
    sourceLines.reset();
    nall::sort(&addressToSourceLineMappings[0], addressToSourceLineMappings.size());

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

      sourceLines.append({ addrToLine.address, sourceLineRaw });
    }
  }
  emit updated();
}

// ------------------------------------------------------------------------
void SymbolMap::revalidate() {
  if (isValid) {
    return;
  }
  
  nall::sort(&labels[0], labels.size());
  nall::sort(&comments[0], comments.size());
  nall::sort(&sourceLines[0], sourceLines.size());
  
  isValid = true;
}

// ------------------------------------------------------------------------
bool SymbolMap::getLabel(uint32_t address, AddressMatch addressMatch, string& outLabel) {
  revalidate();
  return getSymbolData(labels, address, addressMatch, outLabel);
}

// ------------------------------------------------------------------------
bool SymbolMap::getComment(uint32_t address, AddressMatch addressMatch, string& outComment) {
  revalidate();
  return getSymbolData(comments, address, addressMatch, outComment);
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceLine(uint32_t address, AddressMatch addressMatch, string& outSourceLine) {
  revalidate();
  return getSymbolData(sourceLines, address, addressMatch, outSourceLine);
}

// ------------------------------------------------------------------------
int32_t SymbolMap::getSymbolIndexHelper(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch) const
{
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

  // we may not have gotten the exact address, but if "right" and "left" indices are surrounding it, then we want "right"'s result
  // note, though, we want to restrict approximate matching to the same _bank_ (bits 17-24 of addr)
  if (addressMatch == AddressMatch_Closest &&
    right >= 0 && left < symbols.size() &&
    symbols[right].address < address && symbols[left].address > address && 
    (symbols[right].address & 0xFF0000) == (address & 0xFF0000))
  {
    return right;
  }
  return -1;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSymbolData(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch, string& outText) const
{
  int32_t symbolIndex = getSymbolIndexHelper(symbols, address, addressMatch);
  if (symbolIndex != -1)
  {
    outText = symbols[symbolIndex].text;
    return true;
  }

  // dcrooks-todo there may have to be a more expansive search here. We're getting symbols like "NTLR7" because
  // they happen to be close by the _shadowed_ address, but are not correct because we have a more appropriate
  // address elsewhere. Might have to find the best match across the entire space?

  // if there wasn't a match, try the process again, but looking through multiple mirror/shadowed addresses
  // find_mirror_addr will find the next available mirrored address (and loop around at bank FF)
  // but we want to stop if we have looped all of the way around and come up with nothing
  uint32_t mirrorAddr = SNES::bus.find_mirror_addr(address);
  while (mirrorAddr != ~0 && mirrorAddr != address)
  {
    symbolIndex = getSymbolIndexHelper(symbols, mirrorAddr, addressMatch);
    if (symbolIndex != -1)
    {
      outText = symbols[symbolIndex].text;
      return true;
    }

    mirrorAddr = SNES::bus.find_mirror_addr(mirrorAddr);
  }
  return false;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceLineLocationInternal(uint32_t address, AddressMatch addressMatch, uint32_t &outFile, uint32_t &outLine) const
{
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

  // we may not have gotten the exact address, but if "right" and "left" indices are surrounding it, then we want "right"'s result
  if (addressMatch == AddressMatch_Closest &&
    right >= 0 && left < addressToSourceLineMappings.size() &&
    addressToSourceLineMappings[right].address < address && addressToSourceLineMappings[left].address > address &&
    (addressToSourceLineMappings[right].address & 0xFF0000) == (address & 0xFF0000))
  {
    outFile = addressToSourceLineMappings[right].file;
    outLine = addressToSourceLineMappings[right].line;
    return true;
  }

  return false;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceLineLocation(uint32_t address, AddressMatch addressMatch, uint32_t& outFile, uint32_t &outLine)
{
  if (getSourceLineLocationInternal(address, addressMatch, outFile, outLine))
    return true;

  // if there wasn't a match, try the process again, but looking through multiple mirror/shadowed addresses
  // find_mirror_addr will find the next available mirrored address (and loop around at bank FF)
  // but we want to stop if we have looped all of the way around and come up with nothing
  uint32_t mirrorAddr = SNES::bus.find_mirror_addr(address);
  while (mirrorAddr != ~0 && mirrorAddr != address)
  {
    if (getSourceLineLocationInternal(mirrorAddr, addressMatch, outFile, outLine))
    {
      return true;
    }
    mirrorAddr = SNES::bus.find_mirror_addr(mirrorAddr);
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
bool SymbolMap::getSourceAddress(uint32_t file, uint32_t line, AddressMatch addressMatch, uint32_t& outAddress, uint32_t& outLine)
{
  uint32_t closestAddr = 0;
  uint32_t closestLine = 0;
  uint32_t closestLineDelta = UINT_MAX;
  for (unsigned i = 0; i < addressToSourceLineMappings.size(); ++i)
  {
    if (addressToSourceLineMappings[i].file == file)
    {
      if (addressToSourceLineMappings[i].line == line)
      {
        outAddress = addressToSourceLineMappings[i].address;
        outLine = addressToSourceLineMappings[i].line;
        return true;
      }
      else if (addressToSourceLineMappings[i].line > line && addressToSourceLineMappings[i].line - line < closestLineDelta)
      {
        closestLineDelta = addressToSourceLineMappings[i].line - line;
        closestAddr = addressToSourceLineMappings[i].address;
        closestLine = addressToSourceLineMappings[i].line;
      }
    }
  }

  if (addressMatch == AddressMatch_Closest && closestLineDelta != UINT_MAX)
  {
    outAddress = closestAddr;
    outLine = closestLine;
    return true;
  }

  return false;
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
  labels.reset();
  comments.reset();
  sourceLines.reset();
  addressToSourceLineMappings.reset();
  sourceFiles.reset();
  sourceFileLines.reset();
}

// ------------------------------------------------------------------------
