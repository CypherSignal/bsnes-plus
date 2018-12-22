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

  enum BreakpointMemoryBus
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

  enum { 
    Breakpoints = 8,
    SoftBreakCPU = -1,
    SoftBreakSA1 = -2, 
  };

  struct Breakpoint {
    int unique_id = 0;
    bool enabled = false;
    unsigned addr = 0;
    unsigned addr_end = 0; //0 = unused
    signed data = -1;  //-1 = unused
    
    enum class Mode : unsigned {
      Exec = 1 << 0,
      Read = 1 << 1, 
      Write = 1 << 2
    };
    unsigned mode = (unsigned)Mode::Exec;
    
    BreakpointMemoryBus memory_bus = BreakpointMemoryBus::CPUBus;
    unsigned counter = 0;  //number of times breakpoint has been hit since being set

    enum class Source {
      ExternDebug, // this was a breakpoint provided via the externdebug/debug adapter system
      User // this was a breakpoint provided by a user (at some point in time - this could have been reloaded from disc)
    };
    Source source = User;
  };
  static Breakpoint breakpointFromString(const char* desc);
  static Breakpoint breakpointFromString(const char* addr, const char* mode, const char* source);
  static string breakpointToString(Breakpoint bp);

  void breakpoint_test(BreakpointMemoryBus memory_bus, Breakpoint::Mode mode, unsigned addr, uint8 data);

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
  
  int addBreakpoint(Breakpoint newBreakpoint);
  void removeBreakpoint(int breakpointId);

  int getBreakpointHit();
  void setBreakpointHit(int breakpointId);

  // dcrooks-todo this function should probably go away at some point. The accessors/modifiers that follow uses of it are almost guaranteed to be accidentally quadratic.
  nall::linear_vector<int> getBreakpointIdList();

private:
  nall::linear_vector<Breakpoint> m_breakpointList;
  int m_breakpointHitId;
  int m_breakpointUniqueId;
};

extern Debugger debugger;
