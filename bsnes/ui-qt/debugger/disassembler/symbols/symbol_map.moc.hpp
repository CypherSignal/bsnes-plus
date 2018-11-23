#ifndef __SYMBOL_MAP__H__
#define __SYMBOL_MAP__H__

class SymbolFileAdapters;

struct Symbol {
  enum Type { INVALID, LOCATION, COMMENT, SOURCE_LINE };

  static Symbol createInvalid() {
    Symbol s;
    s.type = INVALID;
    return s;
  }

  static Symbol createComment(uint32_t address, const string &name) {
    Symbol s;
    s.type = COMMENT;
    s.address = address;
    s.name = name;
    return s;
  }

  static Symbol createLocation(uint32_t address, const string &name) {
    Symbol s;
    s.type = LOCATION;
    s.address = address;
    s.name = name;
    return s;
  }

  static Symbol createSourceLine(uint32_t address, const string &name) {
    Symbol s;
    s.type = SOURCE_LINE;
    s.address = address;
    s.name = name;
    return s;
  }

  inline bool isInvalid() const {
    return type == INVALID;
  }

  inline bool isSymbol() const {
    return type == LOCATION;
  }

  inline bool isComment() const {
    return type == COMMENT;
  }

  inline bool isSourceLine() const {
    return type == SOURCE_LINE;
  }

  bool operator <(const Symbol &other) {
    return address < other.address;
  }

  uint32_t address;
  string name;
  Type type;
};

struct Symbols {
  typedef nall::linear_vector<Symbol> SymbolList;

  uint32_t address;
  SymbolList symbols;

  Symbol getSymbol();
  Symbol getComment();
  Symbol getSourceLine();

  bool operator <(const Symbols &other) {
    return address < other.address;
  }
};

class SymbolMap : public QObject {
  Q_OBJECT

public:
  SymbolMap();

  // for functions that support it, defines whether address searches should
  // do exact matches, or find the closest-without-going-over
  enum AddressMatch
  {
    AddressMatch_Exact,
    AddressMatch_Closest
  };

  typedef nall::linear_vector<Symbols> SymbolsLists;

  void addLocation(uint32_t address, const string &name);
  void addComment(uint32_t address, const string &name);
  void addSymbol(uint32_t address, const Symbol &name);
  void addCommand(uint32_t id, const string &content);
  void addSourceLine(uint32_t address, uint32_t file, uint32_t line);
  void addSourceFile(uint32_t fileId, uint32_t checksum, const string &includeFilePath);
  void removeSymbol(uint32_t address, Symbol::Type type);
  void finishUpdates();

  void loadFromString(const string &file);
  void loadFromFile(const string &baseName, const string &ext);
  void unloadAll();

  void revalidate();

  Symbol getSymbol(uint32_t address, AddressMatch addressMatch);
  Symbol getComment(uint32_t address, AddressMatch addressMatch);
  Symbol getSourceLine(uint32_t address, AddressMatch addressMatch);
  int32_t getSymbolIndex(uint32_t address, AddressMatch addressMatch);
  bool getSourceLineLocation(uint32_t address, AddressMatch addressMatch, uint32_t& outFile, uint32_t &outLine);
  const char* getSourceLineFromLocation(uint32_t file, uint32_t line);
  const char* getSourceIncludeFilePath(uint32_t file);
  const char* getSourceResolvedFilePath(uint32_t file);
  bool getFileIdFromPath(const char* resolvedFilePath, uint32_t& outFile);
  bool getSourceAddress(uint32_t file, uint32_t line, AddressMatch addressMatch, uint32_t& outAddress, uint32_t& outLine);

  bool isValid;
  SymbolsLists symbols;
  SymbolFileAdapters *adapters;

private:

  struct AddressToSourceLine {
    uint32_t address;
    uint32_t file;
    uint32_t line;

    bool operator <(const AddressToSourceLine &other) { return address < other.address; }
  };
  nall::linear_vector<AddressToSourceLine> addressToSourceLineMappings;

  int32_t getSymbolIndexInternal(uint32_t address, AddressMatch addressMatch) const;
  bool getSourceLineLocationInternal(uint32_t address, AddressMatch addressMatch, uint32_t &outFile, uint32_t &outLine) const;

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
