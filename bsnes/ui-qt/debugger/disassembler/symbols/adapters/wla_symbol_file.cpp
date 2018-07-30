#include "wla_symbol_file.hpp"

// ------------------------------------------------------------------------
const char *WlaSymbolFile::getName() const {
  return "WLA symbol file";
}

// ------------------------------------------------------------------------
const char *WlaSymbolFile::getDescription() const {
  return "WLA-Assembler symbol file format";
}

// ------------------------------------------------------------------------
uint32_t WlaSymbolFile::getFeatures() const {
  return 0
    | SymbolFileInterface::Readable
    | SymbolFileInterface::Symbols
    | SymbolFileInterface::Files
    | SymbolFileInterface::LineMap
  ;
}

// ------------------------------------------------------------------------
string WlaSymbolFile::filteredRow(const string &input) const {
  string row(input);

  row.trim("\r");
  optional<unsigned> comment = row.position(";");
  if (comment) {
    unsigned index = comment();
    if (index == 0) {
      return "";
    }
    row = nall::substr(row, 0, index);
  }

  row.trim(" ");
  return row;
}

// ------------------------------------------------------------------------
int32_t WlaSymbolFile::scoreReadString(const lstring &rows) const {
  if (rows.size() == 0) {
    return -1;
  }

  bool isInLabelsSection = false;

  for (uint32_t i=0; i<rows.size(); i++) {
    const string &row = filteredRow(rows[i]);
    if (row.length() == 0) {
      continue;
    }

    if (row == "[labels]") {
      isInLabelsSection = true;
    } else if (row[0] == '[') {
      isInLabelsSection = false;
    } else if (isInLabelsSection) {
      uint32_t address = (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4));

      if (address > 0) {
        return 1;
      }
    }
  }

  return -1;
}

// ------------------------------------------------------------------------
bool WlaSymbolFile::read(const lstring &rows, SymbolMap *map) const {
  enum Section {
    SECTION_UNKNOWN,
    SECTION_LABELS,
    SECTION_COMMENTS,
    SECTION_DEBUG,
    SECTION_FILES,
    SECTION_SOURCEMAP,
  };

  Section section = SECTION_LABELS;
  for (int i=0; i<rows.size(); i++) {
    string row = filteredRow(rows[i]);
    if (row.length() == 0) {
      continue;
    }

    if (row[0] == '[') {
      if (row == "[labels]") { section = SECTION_LABELS; }
      else if (row == "[comments]") { section = SECTION_COMMENTS; }
      else if (row == "[addr-to-line-mapping]") { section = SECTION_SOURCEMAP; }
      else if (row == "[source-files]") { section = SECTION_FILES; }
      
      else { section = SECTION_UNKNOWN; }
      continue;
    }

    switch (section) {
    case SECTION_LABELS:
      map->addLocation(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::substr(row, 8, row.length() - 8)
      );
      break;

    case SECTION_COMMENTS:
      map->addComment(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::substr(row, 8, row.length() - 8)
      );
      break;

    case SECTION_SOURCEMAP:
      map->addSourceLine(
        (nall::hex(nall::substr(row, 0, 2)) << 16) | nall::hex(nall::substr(row, 3, 4)),
        nall::hex(nall::substr(row, 8, 4)),
        nall::hex(nall::substr(row, 13, 8))
      );
      break;

    case SECTION_FILES:
      map->addSourceFile(
        nall::hex(nall::substr(row, 0, 4)),
        nall::hex(nall::substr(row, 5, 8)),
        nall::substr(row, 14, row.length() - 14)
      );
      break;

    case SECTION_UNKNOWN:
      break;
    }
  }

  return true;
}
