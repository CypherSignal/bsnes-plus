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

  typedef nall::linear_vector<Symbols> SymbolsLists;

  void addLocation(uint32_t address, const string &name);
  void addComment(uint32_t address, const string &name);
  void addSymbol(uint32_t address, const Symbol &name);
  void addCommand(uint32_t id, const string &content);
  void addSourceLine(uint32_t address, uint32_t file, uint32_t line);
  void addSourceFile(uint32_t fileId, uint32_t checksum, const string &filename);
  void removeSymbol(uint32_t address, Symbol::Type type);
  void loadFromString(const string &file);
  void loadFromFile(const string &baseName, const string &ext);
  void saveToFile(const string &baseName, const string &ext);
  void finishUpdates();

  void revalidate();

  int32_t getSymbolIndex(uint32_t address);
  Symbol getSymbol(uint32_t address);
  Symbol getComment(uint32_t address);
  Symbol getSourceLine(uint32_t address);

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

  struct SourceFileInformation {
    string filename;
    unsigned long checksum;
  };
  linear_vector<SourceFileInformation> sourceFiles;
  linear_vector<lstring> sourceFileLines;

signals:
  void updated();

};

#endif
