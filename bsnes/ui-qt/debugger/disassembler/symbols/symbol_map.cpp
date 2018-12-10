#include "symbol_map.moc"

// ------------------------------------------------------------------------
SymbolMap::SymbolMap() {
  isValid = false;
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
void SymbolMap::removeLabel(uint32_t address)
{
  revalidate();
  removeSymbolHelper(labels, address);
  isValid = false;
}

// ------------------------------------------------------------------------
void SymbolMap::removeComment(uint32_t address)
{
  revalidate();
  removeSymbolHelper(comments, address);
  isValid = false;
}

// ------------------------------------------------------------------------
void SymbolMap::removeSymbolHelper(SymbolList& symbols, uint32_t address)
{
  // first, populate the list of mirror addresses to search through
  std::array<uint32_t, 256> mirrorAddresses;
  uint32_t numMirrorAddresses = 0;
  SNES::bus.get_mirror_addresses(address, mirrorAddresses, numMirrorAddresses);

  // search through all mirror addresses, and find a symbol that exactly matches provided address
  for (uint32_t i = 0; i < numMirrorAddresses; ++i)
  {
    int symbolIndex = getSymbolIndexHelper(symbols, mirrorAddresses[i], AddressMatch::AddressMatch_Exact);
    if (symbolIndex != -1)
    {
      symbols.remove(symbolIndex);
      break;
    }
  }
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

  emit updated();
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
const SymbolList& SymbolMap::getLabels()
{
  // note that getting labels does not require a "validated" (i.e. sorted) data set, so do not do a revalidation
  return labels;
}

// ------------------------------------------------------------------------
const SymbolList& SymbolMap::getComments()
{
  // note that getting labels does not require a "validated" (i.e. sorted) data set, so do not do a revalidation
  return comments;
}
// ------------------------------------------------------------------------
bool SymbolMap::getSourceLine(uint32_t address, AddressMatch addressMatch, string& outSourceLine) {
  revalidate();
  return getSymbolData(sourceLines, address, addressMatch, outSourceLine);
}

// ------------------------------------------------------------------------
int SymbolMap::getSymbolIndexHelper(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch) const
{
  int left = 0;
  int right = symbols.size() - 1;

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
  // first, populate the list of mirror addresses to search through
  std::array<uint32_t, 256> mirrorAddresses;
  uint32_t numMirrorAddresses = 0;
  SNES::bus.get_mirror_addresses(address, mirrorAddresses, numMirrorAddresses);

  if (addressMatch == AddressMatch_Closest)
  {
    // search through all mirror addresses, and find which symbol was closest to our provided address
    int closestSymbolIndex = -1;
    uint32_t closestSymbolsDistance = UINT_MAX;
    for (uint32_t i = 0; i < numMirrorAddresses; ++i)
    {
      int symbolIndex = getSymbolIndexHelper(symbols, mirrorAddresses[i], addressMatch);
      if (symbolIndex != -1)
      {
        uint32_t addressDistance = (address & 0xFFFF) - (symbols[symbolIndex].address & 0xFFFF);
        if (addressDistance < closestSymbolsDistance)
        {
          closestSymbolIndex = symbolIndex;
          closestSymbolsDistance = addressDistance;
        }
      }
    }
    if (closestSymbolIndex != -1)
    {
      outText = symbols[closestSymbolIndex].text;
      return true;
    }
  }
  else if (addressMatch == AddressMatch_Exact)
  { 
    // search through all mirror addresses, and find a symbol that exactly matches provided address
    for (uint32_t i = 0; i < numMirrorAddresses; ++i)
    {
      int symbolIndex = getSymbolIndexHelper(symbols, mirrorAddresses[i], addressMatch);
      if (symbolIndex != -1)
      {
        outText = symbols[symbolIndex].text;
        return true;
      }
    }
  }
  return false;
}

// ------------------------------------------------------------------------
bool SymbolMap::getSourceLineLocationHelper(uint32_t address, AddressMatch addressMatch, uint32_t &outFile, uint32_t &outLine) const
{
  int left = 0;
  int right = addressToSourceLineMappings.size() - 1;

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
  // populate the list of mirror addresses to search through
  std::array<uint32_t, 256> mirrorAddresses;
  uint32_t numMirrorAddresses = 0;
  SNES::bus.get_mirror_addresses(address, mirrorAddresses, numMirrorAddresses);

  // then check each of those for a relevant source line
  for (uint32_t i = 0; i < numMirrorAddresses; ++i)
  {
    if (getSourceLineLocationHelper(mirrorAddresses[i], addressMatch, outFile, outLine))
      return true;
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

  enum Section {
    SECTION_UNKNOWN,
    SECTION_LABELS,
    SECTION_COMMENTS,
    SECTION_DEBUG,
    SECTION_FILES,
    SECTION_SOURCEMAP,
  };

  Section section = SECTION_LABELS;
  for (int i = 0; i < rows.size(); i++) {

    // filter the contents of the row in question (e.g. normalize EOL, trailing/leading whitesapce, and commented text)
    string row(rows[i]);

    row.trim("\r");
    optional<unsigned> comment = row.position(";");
    if (comment) {
      unsigned index = comment();
      if (index == 0) {
        continue;
      }
      row = nall::substr(row, 0, index);
    }
    row.trim(" ");
    if (row.length() == 0) {
      continue;
    }
 
    // check if the row is a section header, and change what kind of processing is being done as needed
    if (row[0] == '[') {
      if (row == "[labels]") { section = SECTION_LABELS; }
      else if (row == "[comments]") { section = SECTION_COMMENTS; }
      else if (row == "[addr-to-line mapping]") { section = SECTION_SOURCEMAP; }
      else if (row == "[source files]") { section = SECTION_FILES; }
      else { section = SECTION_UNKNOWN; }
      continue;
    }

    // process the data as appropriate for this section
    switch (section) {
    case SECTION_LABELS:
      addLabel(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::substr(row, 8, row.length() - 8)
      );
      break;

    case SECTION_COMMENTS:
      addComment(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::substr(row, 8, row.length() - 8)
      );
      break;

    case SECTION_SOURCEMAP:
      addSourceLine(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::hex(nall::substr(row, 8, 4)),
        nall::hex(nall::substr(row, 13, 8))
      );
      break;

    case SECTION_FILES:
      addSourceFile(
        nall::hex(nall::substr(row, 0, 4)),
        nall::hex(nall::substr(row, 5, 8)),
        nall::substr(row, 14, row.length() - 14)
      );
      break;

    case SECTION_UNKNOWN:
      break;
    }
  }
  finishUpdates();


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
  isValid = false;
}

// ------------------------------------------------------------------------
