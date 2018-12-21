#include "breakpoint.moc"
BreakpointEditor *breakpointEditor;

BreakpointItem::BreakpointItem(QGridLayout* gridLayout, int row) : m_breakpointId(0) {
  addr = new QLineEdit;
  addr->setFixedWidth(80);
  gridLayout->addWidget(addr, row, BreakAddrStart);
  connect(addr, SIGNAL(textChanged(const QString&)), this, SLOT(init()));
  connect(addr, SIGNAL(textChanged(const QString&)), this, SLOT(toggle()));
  
  QLabel* dashLabel = new QLabel(" - ");
  gridLayout->addWidget(dashLabel, row, BreakAddrDash);

  addr_end = new QLineEdit;
  addr_end->setFixedWidth(80);
  gridLayout->addWidget(addr_end, row, BreakAddrEnd);
  connect(addr_end, SIGNAL(textChanged(const QString&)), this, SLOT(init()));
  connect(addr_end, SIGNAL(textChanged(const QString&)), this, SLOT(toggle()));
  
  data = new QLineEdit;
  data->setFixedWidth(60);
  gridLayout->addWidget(data, row, BreakData);
  connect(data, SIGNAL(textChanged(const QString&)), this, SLOT(init()));
  connect(data, SIGNAL(textChanged(const QString&)), this, SLOT(toggle()));
  
  mode_r = new QCheckBox;
  gridLayout->addWidget(mode_r, row, BreakRead);
  connect(mode_r, SIGNAL(toggled(bool)), this, SLOT(toggle()));
  mode_w = new QCheckBox;
  gridLayout->addWidget(mode_w, row, BreakWrite);
  connect(mode_w, SIGNAL(toggled(bool)), this, SLOT(toggle()));
  mode_x = new QCheckBox;
  gridLayout->addWidget(mode_x, row, BreakExecute);
  connect(mode_x, SIGNAL(toggled(bool)), this, SLOT(toggle()));
  
  source = new QComboBox;
  source->addItem("S-CPU bus");
  source->addItem("S-SMP bus");
  source->addItem("S-PPU VRAM");
  source->addItem("S-PPU OAM");
  source->addItem("S-PPU CGRAM");
  source->addItem("SA-1 bus");
  source->addItem("SuperFX bus");
  gridLayout->addWidget(source, row, BreakSource);
  connect(source, SIGNAL(currentIndexChanged(int)), this, SLOT(init()));
  connect(source, SIGNAL(currentIndexChanged(int)), this, SLOT(toggle()));
  
  init();
}

void BreakpointItem::init() {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    bp.enabled = false;
    bp.counter = 0;
  }
}

bool BreakpointItem::isEnabled() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    return bp.enabled;
  }
  return false;
}

uint32_t BreakpointItem::getAddressFrom() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    return bp.addr;
  }
  return 0;
}

uint32_t BreakpointItem::getAddressTo() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    if (bp.addr_end == 0) {
      return bp.addr;
    }
    else {
      return bp.addr_end;
    }
  }
  return 0;
}

bool BreakpointItem::isModeR() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    return bp.mode & (unsigned)SNES::Debugger::Breakpoint::Mode::Read;
  }
  return false;
}

bool BreakpointItem::isModeW() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    return bp.mode & (unsigned)SNES::Debugger::Breakpoint::Mode::Write;
  }
  return false;
}

bool BreakpointItem::isModeX() const {
  SNES::Debugger::Breakpoint bp;
  if (SNES::debugger.getBreakpoint(m_breakpointId, bp)) {
    return bp.mode & (unsigned)SNES::Debugger::Breakpoint::Mode::Exec;
  }
  return false;
}

string BreakpointItem::getBus() const {
  switch ((SNES::Debugger::BreakpointSourceBus)source->currentIndex()) {
    default:
    case SNES::Debugger::CPUBus: return "cpu";
    case SNES::Debugger::APURAM: return "smp";
    case SNES::Debugger::VRAM: return "vram";
    case SNES::Debugger::OAM: return "oam";
    case SNES::Debugger::CGRAM: return "cgram";
    case SNES::Debugger::SA1Bus: return "sa1";
    case SNES::Debugger::SFXBus: return "sfx";
  }

  return "";
}

void BreakpointItem::toggle() {
  SNES::debugger.removeBreakpoint(m_breakpointId);

  SNES::Debugger::Breakpoint bp;
  bool state = mode_r->isChecked() | mode_w->isChecked() | mode_x->isChecked();
  bp.enabled = state;
  if (state) {
    bp.addr = hex(addr->text().toUtf8().data()) & 0xffffff;
    bp.addr_end = hex(addr_end->text().toUtf8().data()) & 0xffffff;
    if (addr_end->text().length() == 0) {
      bp.addr_end = 0;
    }
    bp.data = hex(data->text().toUtf8().data()) & 0xff;
    if (data->text().length() == 0) {
      bp.data = -1;
    }

    bp.mode = 0;
    bp.mode |= mode_r->isChecked() ? (unsigned)SNES::Debugger::Breakpoint::Mode::Read : 0;
    bp.mode |= mode_w->isChecked() ? (unsigned)SNES::Debugger::Breakpoint::Mode::Write : 0;
    bp.mode |= mode_x->isChecked() ? (unsigned)SNES::Debugger::Breakpoint::Mode::Exec : 0;

    bp.source = (SNES::Debugger::BreakpointSourceBus)source->currentIndex();
    m_breakpointId = SNES::debugger.addBreakpoint(bp);
  }
}

void BreakpointItem::clear() {
  addr->setText("");
  addr_end->setText("");
  data->setText("");
  
  mode_r->setChecked(false);
  mode_w->setChecked(false);
  mode_x->setChecked(false);
  
  source->setCurrentIndex(0);
}

void BreakpointItem::removeBreakpoint() {
  clear();
  toggle();
}

void BreakpointItem::setBreakpoint(string addrStr, string mode, string sourceStr) {
  if (addrStr == "") return;

  sourceStr.lower();
  if(sourceStr == "cpu")        { source->setCurrentIndex(0); }
  else if(sourceStr == "smp")   { source->setCurrentIndex(1); }
  else if(sourceStr == "vram")  { source->setCurrentIndex(2); }
  else if(sourceStr == "oam")   { source->setCurrentIndex(3); }
  else if(sourceStr == "cgram") { source->setCurrentIndex(4); }
  else if(sourceStr == "sa1")   { source->setCurrentIndex(5); }
  else if(sourceStr == "sfx")   { source->setCurrentIndex(6); }
  else { return; }

  mode.lower();
  if(mode.position("r")) { mode_r->setChecked(true); }
  if(mode.position("w")) { mode_w->setChecked(true); }
  if(mode.position("x")) { mode_x->setChecked(true); }

  lstring addresses;
  addresses.split<2>("=", addrStr);
  if (addresses.size() >= 2) { data->setText(addresses[1]); }
  
  addrStr = addresses[0];
  addresses.split<2>("-", addrStr);
  addr->setText(addresses[0]);
  if (addresses.size() >= 2) { addr_end->setText(addresses[1]); }

  toggle();
}

string BreakpointItem::toString() const {
  if (addr->text().isEmpty()) return "";
  
  string breakpoint;
  
  breakpoint << addr->text().toUtf8().data();
  if (!addr_end->text().isEmpty()) {
    breakpoint << "-" << addr_end->text().toUtf8().data();
  }
  if (!data->text().isEmpty()) {
    breakpoint << "=" << data->text().toUtf8().data();
  }
  
  breakpoint << ":";
  if (mode_r->isChecked()) breakpoint << "r";
  if (mode_w->isChecked()) breakpoint << "w";
  if (mode_x->isChecked()) breakpoint << "x";
  
  breakpoint << ":" << getBus();
  
  return breakpoint;
}

BreakpointEditor::BreakpointEditor() {
  setObjectName("breakpoint-editor");
  setWindowTitle("Breakpoint Editor");
  setGeometryString(&config().geometry.breakpointEditor);
  application.windowList.append(this);

  // Generate a widgetitem for the breakpoints, matching its column layout
  QGridLayout* gridLayout = new QGridLayout();
  gridLayout->setSizeConstraint(QLayout::SetFixedSize);
  gridLayout->setMargin(Style::WindowMargin);
  gridLayout->setSpacing(Style::WidgetSpacing);
  setLayout(gridLayout);

  gridLayout->setMargin(Style::WindowMargin);
  gridLayout->setSpacing(Style::WidgetSpacing);

  QLabel *label;
  label = new QLabel("Address Range");
  gridLayout->addWidget(label, 0, BreakpointItem::BreakAddrStart, 1, (BreakpointItem::BreakAddrEnd - BreakpointItem::BreakAddrStart + 1));

  label = new QLabel("Data");
  gridLayout->addWidget(label, 0, BreakpointItem::BreakData);

  label = new QLabel("R");
  label->setAlignment(Qt::AlignHCenter);
  gridLayout->addWidget(label, 0, BreakpointItem::BreakRead);

  label = new QLabel("W");
  label->setAlignment(Qt::AlignHCenter);
  gridLayout->addWidget(label, 0, BreakpointItem::BreakWrite);

  label = new QLabel("X");
  label->setAlignment(Qt::AlignHCenter);
  gridLayout->addWidget(label, 0, BreakpointItem::BreakExecute);

  label = new QLabel("Source");
  gridLayout->addWidget(label, 0, BreakpointItem::BreakSource);

  for(unsigned n = 0; n < SNES::Debugger::Breakpoints; n++) {
    breakpoint[n] = new BreakpointItem(gridLayout, n+1);
  }
  
  breakOnWDM = new QCheckBox("Break on WDM (CPU/SA-1 opcode 0x42)");
  breakOnWDM->setChecked(SNES::debugger.break_on_wdm);
  connect(breakOnWDM, SIGNAL(toggled(bool)), this, SLOT(toggle()));
  gridLayout->addWidget(breakOnWDM, SNES::Debugger::Breakpoints + 1, 0, 1, -1);

  breakOnBRK = new QCheckBox("Break on BRK (CPU/SA-1 opcode 0x00)");
  breakOnBRK->setChecked(SNES::debugger.break_on_brk);
  connect(breakOnBRK, SIGNAL(toggled(bool)), this, SLOT(toggle()));
  gridLayout->addWidget(breakOnBRK, SNES::Debugger::Breakpoints + 2, 0, 1, -1);
}

void BreakpointEditor::toggle() {
  SNES::debugger.break_on_brk = breakOnBRK->isChecked();
  SNES::debugger.break_on_wdm = breakOnWDM->isChecked();
}

void BreakpointEditor::clear() {
  for(unsigned n = 0; n < SNES::Debugger::Breakpoints; n++) {
    breakpoint[n]->clear();
  }
}

void BreakpointEditor::setBreakOnBrk(bool b) {
  breakOnBRK->setChecked(b);
  SNES::debugger.break_on_brk = b;
}

void BreakpointEditor::addBreakpoint(const string& addr, const string& mode, const string& source) {
  for(unsigned n = 0; n < SNES::Debugger::Breakpoints; n++) {
    if(breakpoint[n]->addr->text().isEmpty()) {
      breakpoint[n]->setBreakpoint(addr, mode, source);
      return;
    }
  }
}

void BreakpointEditor::addBreakpoint(const string& breakpoint) {
  lstring param;
  param.split<3>(":", breakpoint);
  if(param.size() == 1) { param.append("rwx"); }
  if(param.size() == 2) { param.append("cpu"); }
  
  this->addBreakpoint(param[0], param[1], param[2]);
}

void BreakpointEditor::removeBreakpoint(uint32_t index) {
  if (index >= SNES::Debugger::Breakpoints) {
    return;
  }

  breakpoint[index]->removeBreakpoint();
}

int32_t BreakpointEditor::indexOfBreakpointExec(uint32_t addr, const string &source) const {
  for(unsigned n = 0; n < SNES::Debugger::Breakpoints; n++) {
    if(breakpoint[n]->isEnabled() && breakpoint[n]->isModeX() && breakpoint[n]->getAddressFrom() <= addr && breakpoint[n]->getAddressTo() >= addr) {
      return n;
    }
  }

  return -1;
}

string BreakpointEditor::toStrings() const {
  string breakpoints;
  
  for(unsigned n = 0; n < SNES::Debugger::Breakpoints; n++) {
    if(!breakpoint[n]->addr->text().isEmpty()) {
      breakpoints << breakpoint[n]->toString() << "\n";
    }
  }
  
  return breakpoints;
}
