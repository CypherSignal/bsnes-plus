class BreakpointItem : public QObject {
  Q_OBJECT

public:
  enum {
    BreakAddrStart = 0,
    BreakAddrDash,
    BreakAddrEnd,
    BreakData,
    BreakRead,
    BreakWrite,
    BreakExecute,
    BreakSource
  };

  QLineEdit *addr;
  QLineEdit *addr_end;
  QLineEdit *data;
  QCheckBox *mode_r;
  QCheckBox *mode_w;
  QCheckBox *mode_x;
  QComboBox *memory_bus;
  BreakpointItem(QGridLayout* gridLayout, int row);

public slots:
  void toggle();
  void clear();

private:
  int m_breakpointId;
};

class BreakpointEditor : public Window {
  Q_OBJECT

public:
  BreakpointItem *breakpoint[SNES::Debugger::Breakpoints];
  QCheckBox *breakOnWDM;
  QCheckBox *breakOnBRK;

  BreakpointEditor();

  void setBreakOnBrk(bool b);

public slots:
  void toggle();
  void clear();
};

extern BreakpointEditor *breakpointEditor;
