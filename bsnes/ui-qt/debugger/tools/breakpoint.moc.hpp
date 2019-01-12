//class BreakpointItem : public QObject {
//  Q_OBJECT
//
//public:
//  enum {
//    BreakAddrStart = 0,
//    BreakAddrDash,
//    BreakAddrEnd,
//    BreakData,
//    BreakRead,
//    BreakWrite,
//    BreakExecute,
//    BreakSource
//  };
//
//  QLineEdit *addr;
//  QLineEdit *addr_end;
//  QLineEdit *data;
//  QCheckBox *mode_r;
//  QCheckBox *mode_w;
//  QCheckBox *mode_x;
//  QComboBox *memory_bus;
//  BreakpointItem(QGridLayout* gridLayout, int row);
//
//public slots:
//  void toggle();
//  void clear();
//
//private:
//  int m_breakpointId;
//};

class BreakpointDialog : public QDialog {
  Q_OBJECT
public:
  static SNES::Debugger::Breakpoint getBreakpoint(bool* ok, QWidget* parent = nullptr);
  BreakpointDialog(QWidget* parent = nullptr);

  // returns the breakpoint specified if the user hits OK
  SNES::Debugger::Breakpoint getBreakpoint() const;
private:
  QLineEdit* addr;
  QLineEdit* addr_end;
  QLineEdit* data;
  QCheckBox* mode_r;
  QCheckBox* mode_w;
  QCheckBox* mode_x;
  QComboBox* memory_bus;

};

class BreakpointEditor : public Window {
  Q_OBJECT

public:
  //BreakpointItem *breakpoint[SNES::Debugger::Breakpoints];
  QPushButton* addBreakpoint;
  QTreeView *treeview;
  QStandardItemModel* breakpointModel;
  QCheckBox *breakOnWDM;
  QCheckBox *breakOnBRK;
  
  BreakpointEditor();

  void setBreakOnBrk(bool b);

public slots:
  void toggle();
  void clear();
};

extern BreakpointEditor *breakpointEditor;
