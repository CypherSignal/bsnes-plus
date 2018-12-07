#ifndef __SYMBOL_MAP__H__
#define __SYMBOL_MAP__H__

struct Symbol {
  uint32_t address;
  string text;

  bool operator <(const Symbol &other) { return address < other.address; }
};

class SymbolMap : public QObject {
  Q_OBJECT

public:
  SymbolMap();

  void addLabel(uint32_t address, const string &name);
  void addComment(uint32_t address, const string &name);
  void removeLabel(uint32_t address);
  void removeComment(uint32_t address);

  void addSourceLine(uint32_t address, uint32_t file, uint32_t line);
  void addSourceFile(uint32_t fileId, uint32_t checksum, const string &includeFilePath);
  void finishUpdates();

  void loadFromString(const string &file);
  void loadFromFile(const string &baseName, const string &ext);
  void unloadAll();

  void revalidate();

  // for functions that support it, defines whether address searches should
  // do exact matches, or find the closest-without-going-over
  enum AddressMatch
  {
    AddressMatch_Exact,
    AddressMatch_Closest
  };

  bool getLabel(uint32_t address, AddressMatch addressMatch, string& outLabel);
  bool getComment(uint32_t address, AddressMatch addressMatch, string& outComment);
  bool getSourceLine(uint32_t address, AddressMatch addressMatch, string& outSourceLine);
  bool getSourceLineLocation(uint32_t address, AddressMatch addressMatch, uint32_t& outFile, uint32_t &outLine);
  const char* getSourceLineFromLocation(uint32_t file, uint32_t line);
  const char* getSourceIncludeFilePath(uint32_t file);
  const char* getSourceResolvedFilePath(uint32_t file);
  bool getFileIdFromPath(const char* resolvedFilePath, uint32_t& outFile);
  bool getSourceAddress(uint32_t file, uint32_t line, AddressMatch addressMatch, uint32_t& outAddress, uint32_t& outLine);

private:

  typedef nall::linear_vector<Symbol> SymbolList;

  bool isValid;
  SymbolList labels;
  SymbolList comments;
  SymbolList sourceLines;
  bool getSymbolData(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch, string& outText) const;
  int getSymbolIndex(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch) const;
  int getSymbolIndexHelper(const SymbolList& symbols, uint32_t address, AddressMatch addressMatch) const;
  void removeSymbolHelper(SymbolList& symbols, uint32_t address);

  struct AddressToSourceLine {
    uint32_t address;
    uint32_t file;
    uint32_t line;
    bool operator <(const AddressToSourceLine &other) { return address < other.address; }
  };
  nall::linear_vector<AddressToSourceLine> addressToSourceLineMappings;

  bool getSourceLineLocationHelper(uint32_t address, AddressMatch addressMatch, uint32_t &outFile, uint32_t &outLine) const;
  bool tryLoadSourceFile(const char* includeFilepath, string& sourceFileData, string& resolvedFilePath);

  struct SourceFileInformation {
    string filename; // filename as it exists from the symbol file
    unsigned long checksum;
    string resolvedFilePath; // filename as it was loaded from disc
  };
  linear_vector<SourceFileInformation> sourceFiles;
  linear_vector<lstring> sourceFileLines;
  lstring sourceFilePaths;

signals:
  void updated();

};

#endif
