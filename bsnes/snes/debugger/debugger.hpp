class Debugger {
public:
  enum class BreakEvent : unsigned {
    None,
    BreakpointHit,
    CPUStep,
    SMPStep,
    SA1Step,
    SFXStep,
  } break_event;

  enum BreakpointSourceBus
  {
    CPUBus = 0,
    APURAM = 1,
    VRAM = 2,
    OAM = 3,
    CGRAM = 4,
    SA1Bus = 5,
    SFXBus = 6,
    Num_SourceBus = 7,
  };

  enum { Breakpoints = 8,
         SoftBreakCPU = -1,
         SoftBreakSA1 = -2, };
  struct Breakpoint {
    int unique_id = 0;
    bool enabled = false;
    unsigned addr = 0;
    unsigned addr_end = 0; //0 = unused
    signed data = -1;  //-1 = unused
    
    enum class Mode : unsigned { Exec = 1, Read = 2, Write = 4 };
    unsigned mode = (unsigned)Mode::Exec;
    
    BreakpointSourceBus source = BreakpointSourceBus::CPUBus;
    unsigned counter = 0;  //number of times breakpoint has been hit since being set
  };

  void breakpoint_test(BreakpointSourceBus source, Breakpoint::Mode mode, unsigned addr, uint8 data);

  bool step_cpu;
  bool step_smp;
  bool step_sa1;
  bool step_sfx;
  bool bus_access;
  bool break_on_wdm;
  bool break_on_brk;

  enum class StepType : unsigned {
    None, StepInto, StepOver, StepOut
  } step_type;
  int call_count;
  bool step_over_new;

  enum class MemorySource : unsigned { CPUBus, APUBus, APURAM, VRAM, OAM, CGRAM, CartROM, CartRAM, SA1Bus, SFXBus };
  uint8 read(MemorySource, unsigned addr);
  void write(MemorySource, unsigned addr, uint8 data);

  Debugger();

  bool getBreakpoint(int breakpointId, Breakpoint& outBreakpoint);
  void setBreakpoint(int breakpointId, const Breakpoint& newBreakpoint);

  int getBreakpointHit();
  void setBreakpointHit(int breakpointId);

private:
  // dcrooks-todo can we do something to guarantee that breakpointVec is always sorted by the breakpoint's unique_id,
  // and therefore any lookups to it can be done through binary search?
  nall::linear_vector<Breakpoint> m_breakpointList;
  int m_breakpointHitId;

};

extern Debugger debugger;
