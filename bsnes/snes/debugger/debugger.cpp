#ifdef SYSTEM_CPP

Debugger debugger;

// dcrooks-todo need to actually test this
Debugger::Breakpoint Debugger::breakpointFromString(const char* desc)
{
  const char* params[3] = {0};
  params[0] = desc;

  if (auto modePos = nall::strpos(params[0], ":"))
  {
    params[1] = &desc[modePos()];
    if (auto sourcePos = nall::strpos(params[1], ":"))
    {
      params[2] = &desc[sourcePos()];
    }
  }
  return breakpointFromString(params[0], params[1], params[2]);
}

// dcrooks-todo need to actually test this
Debugger::Breakpoint Debugger::breakpointFromString(const char* addr, const char* mode, const char* source)
{
  Breakpoint bp;
  char temp[32] = {0};
  
  // "Addr" string can be "xxxx-xxxx=xxxx"
  if (addr)
  {
    nall::strlcpy(temp, addr, 32);

    // sample first characters as hex to get addr (hex short-circuits on non-hex value)
    bp.addr = nall::hex(temp);

    // find '-' and start from there for addr_end
    if (auto addrEndPos = nall::strpos(temp, "-"))
    {
      bp.addr_end = nall::hex(&temp[addrEndPos()]);
    }

    // find '=' and start from there for data
    if (auto dataPos = nall::strpos(temp, "="))
    {
      bp.data = (signed)nall::hex(&temp[dataPos()]);
    }
  }


  if (mode)
  {
    // copy 'mode' into temp and do position checks to set mode bitfield
    nall::strlcpy(temp, mode, 4);
    nall::strlower(temp);

    if (nall::strpos(temp, "x"))
    {
      bp.mode |= (unsigned)Breakpoint::Mode::Exec;
    }
    if (nall::strpos(temp, "r"))
    {
      bp.mode |= (unsigned)Breakpoint::Mode::Read;
    }
    if (nall::strpos(temp, "w"))
    {
      bp.mode |= (unsigned)Breakpoint::Mode::Write;
    }
  }

  // copy 'source' into temp and do hash-switch to determine appropriate sourceBus
  if (source)
  {
    nall::strlcpy(temp, source, 8);
    nall::strlower(temp);
    switch (hashCalc(temp, strlen(temp)))
    {
    case "cpu"_hash:
      bp.memory_bus = BreakpointMemoryBus::CPUBus;
      break;
    case "smp"_hash:
      bp.memory_bus = BreakpointMemoryBus::APURAM;
      break;
    case "vram"_hash:
      bp.memory_bus = BreakpointMemoryBus::VRAM;
      break;
    case "oam"_hash:
      bp.memory_bus = BreakpointMemoryBus::OAM;
      break;
    case "cgram"_hash:
      bp.memory_bus = BreakpointMemoryBus::CGRAM;
      break;
    case "sa1"_hash:
      bp.memory_bus = BreakpointMemoryBus::SA1Bus;
      break;
    case "sfx"_hash:
      bp.memory_bus = BreakpointMemoryBus::SFXBus;
      break;
    }
  }


  return bp;
}

// dcrooks-todo need to actually test this
string Debugger::breakpointToString(Breakpoint bp)
{
  nall::string toReturn;
  toReturn.reserve(64);
  
  {
    char buffer[24];
    snprintf(buffer, 24, "%.6x-%.6x=%.6x", bp.addr, bp.addr_end, (unsigned)bp.data);
    toReturn.append(buffer);
  }

  if (bp.mode)
  {
    toReturn.append(":");
    if (bp.mode & (unsigned)Breakpoint::Mode::Exec)
    {
      toReturn.append("x");
    }
    if (bp.mode & (unsigned)Breakpoint::Mode::Read)
    {
      toReturn.append("r");
    }
    if (bp.mode & (unsigned)Breakpoint::Mode::Write)
    {
      toReturn.append("w");
    }
  }

  switch (bp.memory_bus) {
    default:
    case SNES::Debugger::CPUBus:
      toReturn.append(":cpu");
      break;
    case SNES::Debugger::APURAM:
      toReturn.append(":smp");
      break;
    case SNES::Debugger::VRAM:
      toReturn.append(":vram");
      break;
    case SNES::Debugger::OAM:
      toReturn.append(":oam");
      break;
    case SNES::Debugger::CGRAM:
      toReturn.append(":cgram");
      break;
    case SNES::Debugger::SA1Bus:
      toReturn.append(":sa1");
      break;
    case SNES::Debugger::SFXBus:
      toReturn.append(":sfx");
      break;
  }

  return toReturn;
}

void Debugger::breakpoint_test(Debugger::BreakpointMemoryBus memory_bus, Debugger::Breakpoint::Mode mode, unsigned addr, uint8 data) {
  for(unsigned i = 0; i < m_breakpointList.size(); ++i) {
    const Breakpoint bp = m_breakpointList[i];
    
    if (!bp.enabled) {
      continue;
    }

    if(bp.data != -1 && bp.data != data) {
      continue;
    }

    if (bp.memory_bus != memory_bus) {
      continue;
    }

    if((bp.mode & (unsigned)mode) == 0) {
      continue;
    }
    
    // account for address mirroring on the S-CPU and SA-1 (and other) buses
    // (with 64kb granularity for ranged breakpoints)
    unsigned addr_start = (bp.addr & 0xff0000) | (addr & 0xffff);
    if (addr_start < bp.addr) {
      addr_start += 1<<16;
    }

    unsigned addr_end = bp.addr;
    if (bp.addr_end > bp.addr) {
      addr_end = bp.addr_end;
    }

    if (memory_bus == Debugger::BreakpointMemoryBus::CPUBus) {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (bus.is_mirror(addr_start, addr)) {
          break;
        }
      }
    }
    else if (memory_bus == Debugger::BreakpointMemoryBus::SA1Bus) {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (sa1bus.is_mirror(addr_start, addr)) {
          break;
        }
      }
    }
    else if (memory_bus == Debugger::BreakpointMemoryBus::SFXBus) {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (superfxbus.is_mirror(addr_start, addr)) {
          break;
        }
      }
    }
    else {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (addr_start == addr) {
          break;
        }
      }
    }

    if (addr_start > addr_end) {
      continue;
    }
    
    m_breakpointList[i].counter++;
    m_breakpointHitId = bp.unique_id;
    break_event = BreakEvent::BreakpointHit;
    scheduler.exit(Scheduler::ExitReason::DebuggerEvent);
    break;
  }
}

uint8 Debugger::read(Debugger::MemorySource source, unsigned addr) {
  switch(source) {
    case MemorySource::CPUBus: {
      return bus.read(addr & 0xffffff);
    } break;

    case MemorySource::APUBus: {
      return smp.op_debugread(addr & 0xffff);
    } break;

    case MemorySource::APURAM: {
      return memory::apuram.read(addr & 0xffff);
    } break;

    case MemorySource::VRAM: {
      return memory::vram.read(addr & 0xffff);
    } break;

    case MemorySource::OAM: {
      if(addr & 0x0200) return memory::oam.read(0x0200 + (addr & 0x1f));
      return memory::oam.read(addr & 0x01ff);
    } break;

    case MemorySource::CGRAM: {
      return memory::cgram.read(addr & 0x01ff);
    } break;
    
    case MemorySource::CartROM: {
      if (addr < memory::cartrom.size())
        return memory::cartrom.read(addr & 0xffffff);
    } break;
    
    case MemorySource::CartRAM: {
      if (addr < memory::cartram.size())
        return memory::cartram.read(addr & 0xffffff);
    } break;
    
    case MemorySource::SA1Bus: {
      if (cartridge.has_sa1())
        return sa1bus.read(addr & 0xffffff);
    } break;
    
    case MemorySource::SFXBus: {
      if (cartridge.has_superfx())
        return superfxbus.read(addr & 0xffffff);
    } break;
  }

  return 0x00;
}

void Debugger::write(Debugger::MemorySource source, unsigned addr, uint8 data) {
  switch(source) {
    case MemorySource::CPUBus: {
      bus.write(addr & 0xffffff, data);
    } break;
    
    case MemorySource::APUBus:
    case MemorySource::APURAM: {
      memory::apuram.write(addr & 0xffff, data);
    } break;

    case MemorySource::VRAM: {
      memory::vram.write(addr & 0xffff, data);
    } break;

    case MemorySource::OAM: {
      if(addr & 0x0200) memory::oam.write(0x0200 + (addr & 0x1f), data);
      else memory::oam.write(addr & 0x01ff, data);
    } break;

    case MemorySource::CGRAM: {
      memory::cgram.write(addr & 0x01ff, data);
    } break;
    
    case MemorySource::CartROM: {
      if (addr < memory::cartrom.size()) {
        memory::cartrom.write(addr & 0xffffff, data);
      }
    } break;
    
    case MemorySource::CartRAM: {
      if (addr < memory::cartram.size())
        memory::cartram.write(addr & 0xffffff, data);
    } break;
    
    case MemorySource::SA1Bus: {
      if (cartridge.has_sa1()) sa1bus.write(addr & 0xffffff, data);
    } break;
    
    case MemorySource::SFXBus: {
      if (cartridge.has_superfx()) superfxbus.write(addr & 0xffffff, data);
    } break;
  }
}

Debugger::Debugger() {
  break_event = BreakEvent::None;

  m_breakpointHitId = 0;
  m_breakpointUniqueId = 100;

  step_cpu = false;
  step_smp = false;
  step_sa1 = false;
  step_sfx = false;
  bus_access = false;
  break_on_wdm = false;
  break_on_brk = false;

  step_type = StepType::None;
}

bool Debugger::getBreakpoint(int breakpointId, Breakpoint& outBreakpoint)
{
  if (breakpointId > 0)
  {
    for (int i = 0; i < m_breakpointList.size(); ++i)
    {
      if (m_breakpointList[i].unique_id == breakpointId)
      {
        outBreakpoint = m_breakpointList[i];
        return true;
      }
    }
  }
  else if (breakpointId == SoftBreakCPU || breakpointId == SoftBreakSA1)
  {
    Breakpoint softBp;
    softBp.unique_id = breakpointId;
    softBp.memory_bus = (breakpointId == SoftBreakCPU ? BreakpointMemoryBus::CPUBus : BreakpointMemoryBus::SA1Bus);
    outBreakpoint = softBp;
    return true;
  }
  return false;
}

int Debugger::addBreakpoint(Breakpoint newBreakpoint)
{
  ++m_breakpointUniqueId;
  newBreakpoint.unique_id = m_breakpointUniqueId;
  m_breakpointList.append(newBreakpoint);
  return m_breakpointUniqueId;
}

void Debugger::removeBreakpoint(int breakpointId)
{
  for (int i = 0; i < m_breakpointList.size(); ++i)
  {
    if (m_breakpointList[i].unique_id == breakpointId)
    {
      m_breakpointList.remove(i);
      return;
    }
  }
}

void Debugger::setBreakpointHit(int breakpointId)
{
  m_breakpointHitId = breakpointId;
}

int Debugger::getBreakpointHit()
{
  return m_breakpointHitId;
}


#endif
