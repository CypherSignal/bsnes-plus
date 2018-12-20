#ifdef SYSTEM_CPP

Debugger debugger;

void Debugger::breakpoint_test(Debugger::BreakpointSourceBus source, Debugger::Breakpoint::Mode mode, unsigned addr, uint8 data) {
  for(unsigned i = 0; i < m_breakpointList.size(); ++i) {
    const Breakpoint bp = m_breakpointList[i];
    
    if (!bp.enabled) {
      continue;
    }

    if(bp.data != -1 && bp.data != data) {
      continue;
    }

    if (bp.source != source) {
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

    if (source == Debugger::BreakpointSourceBus::CPUBus) {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (bus.is_mirror(addr_start, addr)) {
          break;
        }
      }
    }
    else if (source == Debugger::BreakpointSourceBus::SA1Bus) {
      for (; addr_start <= addr_end; addr_start += 1 << 16) {
        if (sa1bus.is_mirror(addr_start, addr)) {
          break;
        }
      }
    }
    else if (source == Debugger::BreakpointSourceBus::SFXBus) {
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
    softBp.source = (breakpointId == SoftBreakCPU ? BreakpointSourceBus::CPUBus : BreakpointSourceBus::SA1Bus);
    outBreakpoint = softBp;
    return true;
  }
  return false;
}

void Debugger::setBreakpoint(int breakpointId, const Breakpoint& newBreakpoint)
{
  if (breakpointId > 0)
  {
    for (int i = 0; i < m_breakpointList.size(); ++i)
    {
      if (m_breakpointList[i].unique_id == breakpointId)
      {
        m_breakpointList[i] = newBreakpoint;
        return;
      }
    }

    // if we reached here, we didn't already have a breakpoint matching one in storage
    m_breakpointList.append(newBreakpoint);
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
