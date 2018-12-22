#include "symbolsview.moc"

// ------------------------------------------------------------------------
SymbolsView::SymbolsView(DisasmProcessor *processor) : processor(processor) {
  setObjectName("symbols");
  setWindowTitle("Symbols");
  application.windowList.append(this);

  layout = new QVBoxLayout;
  layout->setMargin(Style::WindowMargin);
  layout->setSpacing(Style::WidgetSpacing);
  setLayout(layout);

  QHBoxLayout *topLayout = new QHBoxLayout();
  layout->addLayout(topLayout);

  search = new QLineEdit();
  topLayout->addWidget(search);

  list = new QTreeWidget;
  list->setColumnCount(2);
  list->setHeaderLabels(QStringList() << "Address" << "Label/Comment");
  list->setFont(QFont(Style::Monospace));
  list->setColumnWidth(0, list->fontMetrics().width("  123456789  "));
  list->setColumnWidth(1, list->fontMetrics().width("  123456789123456789  "));
  list->setAllColumnsShowFocus(true);
  list->sortByColumn(0, Qt::AscendingOrder);
  list->setRootIsDecorated(false);
  list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list->resizeColumnToContents(0);
  layout->addWidget(list);

  resize(400, 500);
  synchronize();

  connect(processor->getSymbols(), SIGNAL(updated()), this, SLOT(synchronize()));
  connect(list, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(bind(QTreeWidgetItem*, int)));
  connect(search, SIGNAL(textChanged(const QString&)), this, SLOT(synchronize()));
}

// ------------------------------------------------------------------------
void SymbolsView::bind(QTreeWidgetItem *item, int value) {
  if (value != 0) {
    return;
  }

  uint32_t address = item->data(0, Qt::UserRole).toUInt();
  bool enable = item->checkState(0);

  // dcrooks-todo probably need to add an accessor to find some breakpoint for this value
  int breakpoint = breakpointEditor->indexOfBreakpointExec(address, processor->getBreakpointBusName());
  if (!enable && breakpoint > 0) {
    SNES::debugger.removeBreakpoint(breakpoint);
  } else if (enable) {
    SNES::debugger.addBreakpoint(SNES::Debugger::breakpointFromString(nall::hex(address), "x", processor->getBreakpointBusName()));
  }
}

// ------------------------------------------------------------------------
void SymbolsView::synchronize() {

  QStringList filterList;
  {
    QString filter = search->text();
    if (filter.length()) {
      filterList = filter.split(" ");
    }
  }

  SymbolMap *symbols = processor->getSymbols();
  const SymbolList& labels = symbols->getLabels();

  QList<QTreeWidgetItem*> itemList;
  itemList.reserve(labels.size());

  for (uint32_t i = 0; i < labels.size(); ++i) {
    const Symbol& sym = labels[i];

    QString itemText((const char*)sym.text);
    bool filtered = false;
    for (QStringList::iterator it = filterList.begin(); it != filterList.end() && !filtered; it++) {
      if (!itemText.contains(*it, Qt::CaseInsensitive)) {
        filtered = true;
      }
    }

    if (filtered) {
      continue;
    }

    int32_t breakpoint = breakpointEditor->indexOfBreakpointExec(sym.address, processor->getBreakpointBusName());

    itemList.push_back(new QTreeWidgetItem());
    auto item = itemList.back();
    item->setData(0, Qt::UserRole, QVariant(sym.address));
    item->setCheckState(0, breakpoint >= 0 ? Qt::Checked : Qt::Unchecked);
    item->setText(0, hex<6, '0'>(sym.address));
    item->setText(1, itemText);
  }

  list->clear();
  list->addTopLevelItems(itemList);
}

// ------------------------------------------------------------------------
