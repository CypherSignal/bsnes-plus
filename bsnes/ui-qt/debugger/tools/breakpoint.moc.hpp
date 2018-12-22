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
  QComboBox *source;
  BreakpointItem(QGridLayout* gridLayout, int row);

  bool isEnabled() const;
  uint32_t getAddressFrom() const;
  uint32_t getAddressTo() const;
  bool isModeR() const;
  bool isModeW() const;
  bool isModeX() const;

public slots:
  void init();
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

  int32_t indexOfBreakpointExec(uint32_t addr, const string &source) const;

public slots:
  void toggle();
  void clear();
};

extern BreakpointEditor *breakpointEditor;
