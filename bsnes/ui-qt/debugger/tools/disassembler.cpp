#include "disassembler.moc"
DisasmWidget *cpuDisassembler;
DisasmWidget *smpDisassembler;
DisasmWidget *sa1Disassembler;
DisasmWidget *sfxDisassembler;
Disassembler *disassembler;

DisasmWidget::DisasmWidget() {
  layout = new QVBoxLayout;
  layout->setMargin(Style::WindowMargin);
  layout->setSpacing(Style::WidgetSpacing);
  setLayout(layout);

  view = new QTextEdit;
  view->setReadOnly(true);
  view->setFont(QFont(Style::Monospace));
  view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view->setMinimumHeight((25 + 1) * view->fontMetrics().height());
  view->setWordWrapMode(QTextOption::NoWrap);
  layout->addWidget(view);
}

Disassembler::Disassembler() {
  setObjectName("disassembler");
  setWindowTitle("Disassembler");
  setGeometryString(&config().geometry.disassembler);
  application.windowList.append(this);

  layout = new QVBoxLayout;
  layout->setMargin(Style::WindowMargin);
  layout->setSpacing(Style::WidgetSpacing);
  setLayout(layout);

  cpuDisassembler = new DisasmWidget;
  smpDisassembler = new DisasmWidget;
  sa1Disassembler = new DisasmWidget;
  sfxDisassembler = new DisasmWidget;

  tab = new QTabWidget;
  tab->addTab(cpuDisassembler, "S-CPU");
  tab->addTab(smpDisassembler, "S-SMP");
  tab->addTab(sa1Disassembler, "SA-1");
  tab->addTab(sfxDisassembler, "SuperFX");
  layout->addWidget(tab);
}

void Disassembler::refresh(Source source, unsigned addr) {
  uint8_t *usage;
  unsigned mask;
  if(source == CPU) { usage = SNES::cpu.usage; mask = (1 << 24) - 1; }
  if(source == SMP) { usage = SNES::smp.usage; mask = (1 << 16) - 1; }
  if(source == SA1) { usage = SNES::sa1.usage; mask = (1 << 24) - 1; }
  if(source == SFX) { usage = SNES::superfx.usage; mask = (1 << 23) - 1; }

  int line[25];
  for(unsigned i = 0; i < 25; i++) line[i] = -1;

  line[12] = addr;

  for(signed index = 11; index >= 0; index--) {
    int base = line[index + 1];
    if(base == -1) break;

    for(unsigned i = 1; i <= 4; i++) {
      if(usage[(base - i) & mask] & 0x10) {
        line[index] = base - i;
        break;
      }
    }
  }

  for(signed index = 13; index <= 24; index++) {
    int base = line[index - 1];
    if(base == -1) break;

    for(unsigned i = 1; i <= 4; i++) {
      if(usage[(base + i) & mask] & 0x10) {
        line[index] = base + i;
        break;
      }
    }
  }
  
  string htmlOutput;
  htmlOutput << "<table>";
  unsigned int predictedSourceLocFile = ~0;
  unsigned int predictedSourceLocLine = ~0;
  string opCodeHtml;
  for (unsigned i = 0; i < 25; i++) {

    htmlOutput << "<tr><td width=\"250\">";
    
    opCodeHtml = "";
    if (i < 12)
      opCodeHtml << "<font color='#0000a0'>";
    else if (i == 12)
      opCodeHtml << "<font color='#00a000'>";
    else
      opCodeHtml = "<font color='#a00000'>";

    if (line[i] == -1) {
      opCodeHtml << "...";
    }
    else {
      char opCodeText[128];
      if (source == CPU) { SNES::cpu.disassemble_opcode(opCodeText, line[i]); opCodeText[20] = 0; }
      if (source == SMP) { SNES::smp.disassemble_opcode(opCodeText, line[i]); opCodeText[23] = 0; }
      if (source == SA1) { SNES::sa1.disassemble_opcode(opCodeText, line[i]); opCodeText[20] = 0; }
      if (source == SFX) { SNES::superfx.disassemble_opcode(opCodeText, line[i]); opCodeText[25] = 0; }
      rtrim(opCodeText);
      opCodeHtml << "<pre>" << opCodeText << "</pre>";
    }
    opCodeHtml << "</font>";

    SymbolMap *symbols = debugger->getSymbols(source);
    unsigned int currentSourceLocFile = 0;
    unsigned int currentSourceLocLine = 0;

    if (symbols != nullptr && symbols->getSourceLineLocation(line[i], currentSourceLocFile, currentSourceLocLine)) {
      // re-set the previous line location in case we changed files, or jumped around the same file
      if (predictedSourceLocFile != currentSourceLocFile || (predictedSourceLocLine < currentSourceLocLine - 5 || predictedSourceLocLine > currentSourceLocLine)) {
        predictedSourceLocFile = currentSourceLocFile;
        predictedSourceLocLine = currentSourceLocLine;

        const char* file = symbols->getSourceIncludeFilePath(currentSourceLocFile);
        char lineString[256];
        snprintf(lineString, 256, "</td><td>---------- %s", file ? file : "???");
        htmlOutput << lineString << "</td></tr><tr><td>";
      }

      // add source lines consecutively until we're caught up in case we skipped over a few
      while (predictedSourceLocLine != currentSourceLocLine) {
        char opCodeSuffix[256];
        const char* sourceLine = symbols->getSourceLineFromLocation(currentSourceLocFile, predictedSourceLocLine);
        snprintf(opCodeSuffix, 256, "%04d: %s", predictedSourceLocLine, sourceLine ? sourceLine : "???");
        htmlOutput << "</td><td><pre>" << opCodeSuffix << "</pre></td></tr><tr><td>";
        predictedSourceLocLine++;
      }

      // add the actual opcode line and its sourceline suffix
      htmlOutput << opCodeHtml << "</td><td>";

      char opCodeSuffix[256];
      const char* sourceLine = symbols->getSourceLineFromLocation(currentSourceLocFile, currentSourceLocLine);
      snprintf(opCodeSuffix, 256, "%04d: %s", currentSourceLocLine, sourceLine ? sourceLine : "???");
      htmlOutput << "<pre>" << opCodeSuffix << "</pre>";
      predictedSourceLocLine++;
    }
    else {
      htmlOutput << opCodeHtml;
    }
    htmlOutput << "</td></tr>";

  }
  htmlOutput << "</table>";

  if(source == CPU) cpuDisassembler->view->setHtml(htmlOutput);
  if(source == SMP) smpDisassembler->view->setHtml(htmlOutput);
  if(source == SA1) sa1Disassembler->view->setHtml(htmlOutput);
  if(source == SFX) sfxDisassembler->view->setHtml(htmlOutput);
}
