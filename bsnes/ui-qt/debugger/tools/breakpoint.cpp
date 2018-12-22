#include "breakpoint.moc"
BreakpointEditor *breakpointEditor;

BreakpointItem::BreakpointItem(QGridLayout* gridLayout, int row) : m_breakpointId(0) {
  addr = new QLineEdit;
  addr->setFixedWidth(80);
  gridLayout->addWidget(addr, row, BreakAddrStart);
  connect(addr, SIGNAL(textChanged(const QString&)), this, SLOT(toggle()));
  
  QLabel* dashLabel = new QLabel(" - ");
  gridLayout->addWidget(dashLabel, row, BreakAddrDash);

  addr_end = new QLineEdit;
  addr_end->setFixedWidth(80);
  gridLayout->addWidget(addr_end, row, BreakAddrEnd);
  connect(addr_end, SIGNAL(textChanged(const QString&)), this, SLOT(toggle()));
  
  data = new QLineEdit;
  data->setFixedWidth(60);
  gridLayout->addWidget(data, row, BreakData);
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
  
  memory_bus = new QComboBox;
  memory_bus->addItem("S-CPU bus");
  memory_bus->addItem("S-SMP bus");
  memory_bus->addItem("S-PPU VRAM");
  memory_bus->addItem("S-PPU OAM");
  memory_bus->addItem("S-PPU CGRAM");
  memory_bus->addItem("SA-1 bus");
  memory_bus->addItem("SuperFX bus");
  gridLayout->addWidget(memory_bus, row, BreakSource);
  connect(memory_bus, SIGNAL(currentIndexChanged(int)), this, SLOT(toggle()));
}

void BreakpointItem::toggle() {
  SNES::debugger.removeBreakpoint(m_breakpointId);

  bool state = mode_r->isChecked() | mode_w->isChecked() | mode_x->isChecked();
  if (state) {
    SNES::Debugger::Breakpoint bp;
    bp.enabled = state;
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

    bp.memory_bus = (SNES::Debugger::BreakpointMemoryBus)memory_bus->currentIndex();
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
  
  memory_bus->setCurrentIndex(0);
}

//////////////////////////////////////////////////////////////////////////

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

