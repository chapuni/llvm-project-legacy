//===-- NativeRegisterContextLinux_arm64.cpp --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#if defined (__arm64__) || defined (__aarch64__)

#include "NativeRegisterContextLinux_arm64.h"

// C Includes
// C++ Includes

// Other libraries and framework includes
#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Host/common/NativeProcessProtocol.h"

#include "Plugins/Process/Linux/NativeProcessLinux.h"
#include "Plugins/Process/Linux/Procfs.h"
#include "Plugins/Process/Utility/RegisterContextLinux_arm64.h"

// System includes - They have to be included after framework includes because they define some
// macros which collide with variable names in other modules
#include <sys/socket.h>
// NT_PRSTATUS and NT_FPREGSET definition
#include <elf.h>

#define REG_CONTEXT_SIZE (GetGPRSize() + GetFPRSize())

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::process_linux;

// ARM64 general purpose registers.
static const uint32_t g_gpr_regnums_arm64[] =
{
    gpr_x0_arm64,
    gpr_x1_arm64,
    gpr_x2_arm64,
    gpr_x3_arm64,
    gpr_x4_arm64,
    gpr_x5_arm64,
    gpr_x6_arm64,
    gpr_x7_arm64,
    gpr_x8_arm64,
    gpr_x9_arm64,
    gpr_x10_arm64,
    gpr_x11_arm64,
    gpr_x12_arm64,
    gpr_x13_arm64,
    gpr_x14_arm64,
    gpr_x15_arm64,
    gpr_x16_arm64,
    gpr_x17_arm64,
    gpr_x18_arm64,
    gpr_x19_arm64,
    gpr_x20_arm64,
    gpr_x21_arm64,
    gpr_x22_arm64,
    gpr_x23_arm64,
    gpr_x24_arm64,
    gpr_x25_arm64,
    gpr_x26_arm64,
    gpr_x27_arm64,
    gpr_x28_arm64,
    gpr_fp_arm64,
    gpr_lr_arm64,
    gpr_sp_arm64,
    gpr_pc_arm64,
    gpr_cpsr_arm64,
    LLDB_INVALID_REGNUM // register sets need to end with this flag
};
static_assert(((sizeof g_gpr_regnums_arm64 / sizeof g_gpr_regnums_arm64[0]) - 1) == k_num_gpr_registers_arm64, \
              "g_gpr_regnums_arm64 has wrong number of register infos");

// ARM64 floating point registers.
static const uint32_t g_fpu_regnums_arm64[] =
{
    fpu_v0_arm64,
    fpu_v1_arm64,
    fpu_v2_arm64,
    fpu_v3_arm64,
    fpu_v4_arm64,
    fpu_v5_arm64,
    fpu_v6_arm64,
    fpu_v7_arm64,
    fpu_v8_arm64,
    fpu_v9_arm64,
    fpu_v10_arm64,
    fpu_v11_arm64,
    fpu_v12_arm64,
    fpu_v13_arm64,
    fpu_v14_arm64,
    fpu_v15_arm64,
    fpu_v16_arm64,
    fpu_v17_arm64,
    fpu_v18_arm64,
    fpu_v19_arm64,
    fpu_v20_arm64,
    fpu_v21_arm64,
    fpu_v22_arm64,
    fpu_v23_arm64,
    fpu_v24_arm64,
    fpu_v25_arm64,
    fpu_v26_arm64,
    fpu_v27_arm64,
    fpu_v28_arm64,
    fpu_v29_arm64,
    fpu_v30_arm64,
    fpu_v31_arm64,
    fpu_fpsr_arm64,
    fpu_fpcr_arm64,
    LLDB_INVALID_REGNUM // register sets need to end with this flag
};
static_assert(((sizeof g_fpu_regnums_arm64 / sizeof g_fpu_regnums_arm64[0]) - 1) == k_num_fpr_registers_arm64, \
              "g_fpu_regnums_arm64 has wrong number of register infos");

namespace {
    // Number of register sets provided by this context.
    enum
    {
        k_num_register_sets = 2
    };
}

// Register sets for ARM64.
static const RegisterSet
g_reg_sets_arm64[k_num_register_sets] =
{
    { "General Purpose Registers",  "gpr", k_num_gpr_registers_arm64, g_gpr_regnums_arm64 },
    { "Floating Point Registers",   "fpu", k_num_fpr_registers_arm64, g_fpu_regnums_arm64 }
};

NativeRegisterContextLinux*
NativeRegisterContextLinux::CreateHostNativeRegisterContextLinux(const ArchSpec& target_arch,
                                                                 NativeThreadProtocol &native_thread,
                                                                 uint32_t concrete_frame_idx)
{
    return new NativeRegisterContextLinux_arm64(target_arch, native_thread, concrete_frame_idx);
}

NativeRegisterContextLinux_arm64::NativeRegisterContextLinux_arm64 (const ArchSpec& target_arch,
                                                                    NativeThreadProtocol &native_thread,
                                                                    uint32_t concrete_frame_idx) :
    NativeRegisterContextLinux (native_thread, concrete_frame_idx, new RegisterContextLinux_arm64(target_arch))
{
    switch (target_arch.GetMachine())
    {
        case llvm::Triple::aarch64:
            m_reg_info.num_registers     = k_num_registers_arm64;
            m_reg_info.num_gpr_registers = k_num_gpr_registers_arm64;
            m_reg_info.num_fpr_registers = k_num_fpr_registers_arm64;
            m_reg_info.last_gpr          = k_last_gpr_arm64;
            m_reg_info.first_fpr         = k_first_fpr_arm64;
            m_reg_info.last_fpr          = k_last_fpr_arm64;
            m_reg_info.first_fpr_v       = fpu_v0_arm64;
            m_reg_info.last_fpr_v        = fpu_v31_arm64;
            m_reg_info.gpr_flags         = gpr_cpsr_arm64;
            break;
        default:
            assert(false && "Unhandled target architecture.");
            break;
    }

    ::memset(&m_fpr, 0, sizeof (m_fpr));
    ::memset(&m_gpr_arm64, 0, sizeof (m_gpr_arm64));
    ::memset(&m_hwp_regs, 0, sizeof (m_hwp_regs));

    // 16 is just a maximum value, query hardware for actual watchpoint count
    m_max_hwp_supported = 16;
    m_max_hbp_supported = 16;
    m_refresh_hwdebug_info = true;
}

uint32_t
NativeRegisterContextLinux_arm64::GetRegisterSetCount () const
{
    return k_num_register_sets;
}

const RegisterSet *
NativeRegisterContextLinux_arm64::GetRegisterSet (uint32_t set_index) const
{
    if (set_index < k_num_register_sets)
        return &g_reg_sets_arm64[set_index];

    return nullptr;
}

uint32_t
NativeRegisterContextLinux_arm64::GetUserRegisterCount() const
{
    uint32_t count = 0;
    for (uint32_t set_index = 0; set_index < k_num_register_sets; ++set_index)
        count += g_reg_sets_arm64[set_index].num_registers;
    return count;
}

Error
NativeRegisterContextLinux_arm64::ReadRegister (const RegisterInfo *reg_info, RegisterValue &reg_value)
{
    Error error;

    if (!reg_info)
    {
        error.SetErrorString ("reg_info NULL");
        return error;
    }

    const uint32_t reg = reg_info->kinds[lldb::eRegisterKindLLDB];

    if (IsFPR(reg))
    {
        error = ReadFPR();
        if (error.Fail())
            return error;
    }
    else
    {
        uint32_t full_reg = reg;
        bool is_subreg = reg_info->invalidate_regs && (reg_info->invalidate_regs[0] != LLDB_INVALID_REGNUM);

        if (is_subreg)
        {
            // Read the full aligned 64-bit register.
            full_reg = reg_info->invalidate_regs[0];
        }

        error = ReadRegisterRaw(full_reg, reg_value);

        if (error.Success ())
        {
            // If our read was not aligned (for ah,bh,ch,dh), shift our returned value one byte to the right.
            if (is_subreg && (reg_info->byte_offset & 0x1))
                reg_value.SetUInt64(reg_value.GetAsUInt64() >> 8);

            // If our return byte size was greater than the return value reg size, then
            // use the type specified by reg_info rather than the uint64_t default
            if (reg_value.GetByteSize() > reg_info->byte_size)
                reg_value.SetType(reg_info);
        }
        return error;
    }

    // Get pointer to m_fpr variable and set the data from it.
    assert (reg_info->byte_offset < sizeof m_fpr);
    uint8_t *src = (uint8_t *)&m_fpr + reg_info->byte_offset;
    reg_value.SetFromMemoryData(reg_info, src, reg_info->byte_size, eByteOrderLittle, error);

    return error;
}

Error
NativeRegisterContextLinux_arm64::WriteRegister (const RegisterInfo *reg_info, const RegisterValue &reg_value)
{
    if (!reg_info)
        return Error ("reg_info NULL");

    const uint32_t reg_index = reg_info->kinds[lldb::eRegisterKindLLDB];
    if (reg_index == LLDB_INVALID_REGNUM)
        return Error ("no lldb regnum for %s", reg_info && reg_info->name ? reg_info->name : "<unknown register>");

    if (IsGPR(reg_index))
        return WriteRegisterRaw(reg_index, reg_value);

    if (IsFPR(reg_index))
    {
        // Get pointer to m_fpr variable and set the data to it.
        assert (reg_info->byte_offset < sizeof(m_fpr));
        uint8_t *dst = (uint8_t *)&m_fpr + reg_info->byte_offset;
        switch (reg_info->byte_size)
        {
            case 2:
                *(uint16_t *)dst = reg_value.GetAsUInt16();
                break;
            case 4:
                *(uint32_t *)dst = reg_value.GetAsUInt32();
                break;
            case 8:
                *(uint64_t *)dst = reg_value.GetAsUInt64();
                break;
            default:
                assert(false && "Unhandled data size.");
                return Error ("unhandled register data size %" PRIu32, reg_info->byte_size);
        }

        Error error = WriteFPR();
        if (error.Fail())
            return error;

        return Error ();
    }

    return Error ("failed - register wasn't recognized to be a GPR or an FPR, write strategy unknown");
}

Error
NativeRegisterContextLinux_arm64::ReadAllRegisterValues (lldb::DataBufferSP &data_sp)
{
    Error error;

    data_sp.reset (new DataBufferHeap (REG_CONTEXT_SIZE, 0));
    if (!data_sp)
        return Error ("failed to allocate DataBufferHeap instance of size %" PRIu64, REG_CONTEXT_SIZE);

    error = ReadGPR();
    if (error.Fail())
        return error;

    error = ReadFPR();
    if (error.Fail())
        return error;

    uint8_t *dst = data_sp->GetBytes ();
    if (dst == nullptr)
    {
        error.SetErrorStringWithFormat ("DataBufferHeap instance of size %" PRIu64 " returned a null pointer", REG_CONTEXT_SIZE);
        return error;
    }

    ::memcpy (dst, &m_gpr_arm64, GetGPRSize());
    dst += GetGPRSize();
    ::memcpy (dst, &m_fpr, sizeof(m_fpr));

    return error;
}

Error
NativeRegisterContextLinux_arm64::WriteAllRegisterValues (const lldb::DataBufferSP &data_sp)
{
    Error error;

    if (!data_sp)
    {
        error.SetErrorStringWithFormat ("NativeRegisterContextLinux_x86_64::%s invalid data_sp provided", __FUNCTION__);
        return error;
    }

    if (data_sp->GetByteSize () != REG_CONTEXT_SIZE)
    {
        error.SetErrorStringWithFormat ("NativeRegisterContextLinux_x86_64::%s data_sp contained mismatched data size, expected %" PRIu64 ", actual %" PRIu64, __FUNCTION__, REG_CONTEXT_SIZE, data_sp->GetByteSize ());
        return error;
    }


    uint8_t *src = data_sp->GetBytes ();
    if (src == nullptr)
    {
        error.SetErrorStringWithFormat ("NativeRegisterContextLinux_x86_64::%s DataBuffer::GetBytes() returned a null pointer", __FUNCTION__);
        return error;
    }
    ::memcpy (&m_gpr_arm64, src, GetRegisterInfoInterface ().GetGPRSize ());

    error = WriteGPR();
    if (error.Fail())
        return error;

    src += GetRegisterInfoInterface ().GetGPRSize ();
    ::memcpy (&m_fpr, src, sizeof(m_fpr));

    error = WriteFPR();
    if (error.Fail())
        return error;

    return error;
}

bool
NativeRegisterContextLinux_arm64::IsGPR(unsigned reg) const
{
    return reg <= m_reg_info.last_gpr;   // GPR's come first.
}

bool
NativeRegisterContextLinux_arm64::IsFPR(unsigned reg) const
{
    return (m_reg_info.first_fpr <= reg && reg <= m_reg_info.last_fpr);
}

uint32_t
NativeRegisterContextLinux_arm64::SetHardwareBreakpoint (lldb::addr_t addr, size_t size)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    NativeProcessProtocolSP process_sp (m_thread.GetProcess ());
    if (!process_sp)
        return false;

    // Check if our hardware breakpoint and watchpoint information is updated.
    if (m_refresh_hwdebug_info)
    {
        ReadHardwareDebugInfo (m_max_hwp_supported, m_max_hbp_supported);
        m_refresh_hwdebug_info = false;
    }

    uint32_t control_value, bp_index;

    // Check if size has a valid hardware breakpoint length.
    if (size != 4)
        return LLDB_INVALID_INDEX32;  // Invalid size for a AArch64 hardware breakpoint

    // Check 4-byte alignment for hardware breakpoint target address.
    if (addr & 0x03)
        return LLDB_INVALID_INDEX32; // Invalid address, should be 4-byte aligned.

    // Setup control value
    control_value = 0;
    control_value |= ((1 << size) - 1) << 5;
    control_value |= (2 << 1) | 1;

    // Iterate over stored hardware breakpoints
    // Find a free bp_index or update reference count if duplicate.
    bp_index = LLDB_INVALID_INDEX32;
    for (uint32_t i = 0; i < m_max_hbp_supported; i++)
    {
        if ((m_hbr_regs[i].control & 1) == 0)
        {
            bp_index = i;  // Mark last free slot
        }
        else if (m_hbr_regs[i].address == addr && m_hbr_regs[i].control == control_value)
        {
            bp_index = i;  // Mark duplicate index
            break;  // Stop searching here
        }
    }

     if (bp_index == LLDB_INVALID_INDEX32)
        return LLDB_INVALID_INDEX32;

    // Add new or update existing watchpoint
    if ((m_hbr_regs[bp_index].control & 1) == 0)
    {
        m_hbr_regs[bp_index].address = addr;
        m_hbr_regs[bp_index].control = control_value;
        m_hbr_regs[bp_index].refcount = 1;

        //TODO: PTRACE CALL HERE for an UPDATE
    }
    else
        m_hbr_regs[bp_index].refcount++;

    return bp_index;
}

bool
NativeRegisterContextLinux_arm64::ClearHardwareBreakpoint (uint32_t hw_idx)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    if (hw_idx >= m_max_hbp_supported)
        return false;

    // Update reference count if multiple references.
    if (m_hbr_regs[hw_idx].refcount > 1)
    {
        m_hbr_regs[hw_idx].refcount--;
        return true;
    }
    else if (m_hbr_regs[hw_idx].refcount == 1)
    {
        m_hbr_regs[hw_idx].control &= ~1;
        m_hbr_regs[hw_idx].address = 0;
        m_hbr_regs[hw_idx].refcount = 0;

        //TODO: PTRACE CALL HERE for an UPDATE
        return true;
    }

    return false;
}

uint32_t
NativeRegisterContextLinux_arm64::NumSupportedHardwareWatchpoints ()
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    return m_max_hwp_supported;
}

uint32_t
NativeRegisterContextLinux_arm64::SetHardwareWatchpoint (lldb::addr_t addr, size_t size, uint32_t watch_flags)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    NativeProcessProtocolSP process_sp (m_thread.GetProcess ());
    if (!process_sp)
        return false;

    
    // Check if our hardware breakpoint and watchpoint information is updated.
    if (m_refresh_hwdebug_info)
    {
        ReadHardwareDebugInfo (m_max_hwp_supported, m_max_hbp_supported);
        m_refresh_hwdebug_info = false;
    }
		
    uint32_t control_value, wp_index;


    if (watch_flags != 0x1 && watch_flags != 0x2 && watch_flags != 0x3)
        return 0;//Error ("Invalid read/write bits for watchpoint");

    // Check if size has a valid hardware watchpoint length.
    if (size != 1 && size != 2 && size != 4 && size != 8)
        return 0;//Error ("Invalid size for watchpoint");

    // Check 8-byte alignment for hardware watchpoint target address.
    // TODO: Add support for watching un-aligned addresses
    if (addr & 0x07)
        return 0;//Error ("LLDB for AArch64 currently supports 8-byte alignment for hardware watchpoint target address.");

    // Setup control value
    control_value = watch_flags << 3;
    control_value |= ((1 << size) - 1) << 5;
    control_value |= (2 << 1) | 1;

    // Iterate over stored watchpoints
    // Find a free wp_index or update reference count if duplicate.
    wp_index = LLDB_INVALID_INDEX32;
    for (uint32_t i = 0; i < m_max_hwp_supported; i++)
    {
        if ((m_hwp_regs[i].control & 1) == 0)
        {
            wp_index = i; // Mark last free slot
        }
        else if (m_hwp_regs[i].address == addr && m_hwp_regs[i].control == control_value)
        {
            wp_index = i; // Mark duplicate index
            break; // Stop searching here
        }
    }

     if (wp_index == LLDB_INVALID_INDEX32)
        return LLDB_INVALID_INDEX32;

    // Add new or update existing watchpoint
    if ((m_hwp_regs[wp_index].control & 1) == 0)
    {
        m_hwp_regs[wp_index].address = addr;
        m_hwp_regs[wp_index].control = control_value;
        m_hwp_regs[wp_index].refcount = 1;

        // PTRACE call to set corresponding watchpoint register.
        WriteHardwareDebugRegs(&addr, &control_value, 0, wp_index);
    }
    else
        m_hwp_regs[wp_index].refcount++;

    return wp_index;
}

bool
NativeRegisterContextLinux_arm64::ClearHardwareWatchpoint (uint32_t wp_index)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    NativeProcessProtocolSP process_sp (m_thread.GetProcess ());
    if (!process_sp)
        return false;

    if (wp_index >= m_max_hwp_supported)
        return false;

    // Update reference count if multiple references.
    if (m_hwp_regs[wp_index].refcount > 1)
    {
        m_hwp_regs[wp_index].refcount--;
        return true;
    }
    else if (m_hwp_regs[wp_index].refcount == 1)
    {
        m_hwp_regs[wp_index].control &= ~1;
        m_hwp_regs[wp_index].address = 0;
        m_hwp_regs[wp_index].refcount = 0;

        //TODO: PTRACE CALL HERE for an UPDATE
        WriteHardwareDebugRegs(&m_hwp_regs[wp_index].address, &m_hwp_regs[wp_index].control, 0, wp_index);
        return true;
    }

    return false;
}

Error
NativeRegisterContextLinux_arm64::ClearAllHardwareWatchpoints ()
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    NativeProcessProtocolSP process_sp (m_thread.GetProcess ());

    Error ml_error;
    ml_error.SetErrorToErrno();
    if (!process_sp)
        return ml_error;

    for (uint32_t i = 0; i < m_max_hwp_supported; i++)
    {
        if (m_hwp_regs[i].control & 0x01)
        {
            m_hwp_regs[i].control &= ~1;
            m_hwp_regs[i].address = 0;
            m_hwp_regs[i].refcount = 0;

            WriteHardwareDebugRegs(&m_hwp_regs[i].address, &m_hwp_regs[i].control, 0, i);
        }
    }

    return Error();
}

uint32_t
NativeRegisterContextLinux_arm64::GetWatchpointSize(uint32_t wp_index)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);
    switch ((m_hwp_regs[wp_index].control >> 5) & 0xff)
    {
        case 0x01:
            return 1;
        case 0x03:
            return 2;
        case 0x0f:
            return 4;
        case 0xff:
            return 8;
        default:
            return 0;
    }
}
bool
NativeRegisterContextLinux_arm64::WatchpointIsEnabled(uint32_t wp_index)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    if ((m_hwp_regs[wp_index].control & 0x1) == 0x1)
        return true;
    else
        return false;
}

Error
NativeRegisterContextLinux_arm64::GetWatchpointHitIndex(uint32_t &wp_index, lldb::addr_t trap_addr)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    uint32_t watch_size;
    lldb::addr_t watch_addr;

    for (wp_index = 0; wp_index < m_max_hwp_supported; ++wp_index)
    {
        watch_size = GetWatchpointSize (wp_index);
        watch_addr = m_hwp_regs[wp_index].address;

        if (m_hwp_regs[wp_index].refcount >= 1 && WatchpointIsEnabled(wp_index)
            && trap_addr >= watch_addr && trap_addr < watch_addr + watch_size)
        {
            return Error();
        }
    }

    wp_index = LLDB_INVALID_INDEX32;
    return Error();
}

lldb::addr_t
NativeRegisterContextLinux_arm64::GetWatchpointAddress (uint32_t wp_index)
{
    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS));

    if (log)
        log->Printf ("NativeRegisterContextLinux_arm64::%s()", __FUNCTION__);

    if (wp_index >= m_max_hwp_supported)
        return LLDB_INVALID_ADDRESS;

    if (WatchpointIsEnabled(wp_index))
        return m_hwp_regs[wp_index].address;
    else
        return LLDB_INVALID_ADDRESS;
}

Error
NativeRegisterContextLinux_arm64::ReadHardwareDebugInfo(unsigned int &watch_count,
                                                        unsigned int &break_count)
{
    NativeProcessProtocolSP process_sp (m_thread.GetProcess());
    if (!process_sp)
        return Error("NativeProcessProtocol is NULL");
    NativeProcessLinux *const process_p = reinterpret_cast<NativeProcessLinux*>(process_sp.get());
    ::pid_t tid = m_thread.GetID();

    return process_p->DoOperation([&] {
        int regset = NT_ARM_HW_WATCH;
        struct iovec ioVec;
        struct user_hwdebug_state dreg_state;
        Error error;

        ioVec.iov_base = &dreg_state;
        ioVec.iov_len = sizeof (dreg_state);
        error = NativeProcessLinux::PtraceWrapper(PTRACE_GETREGSET, tid, &regset, &ioVec, ioVec.iov_len);
        watch_count = dreg_state.dbg_info & 0xff;

        regset = NT_ARM_HW_BREAK;
        error = NativeProcessLinux::PtraceWrapper(PTRACE_GETREGSET, tid, &regset, &ioVec, ioVec.iov_len);
        break_count = dreg_state.dbg_info & 0xff;

        return error;
    });
}

Error
NativeRegisterContextLinux_arm64::WriteHardwareDebugRegs(lldb::addr_t *addr_buf,
                                                         uint32_t *cntrl_buf,
                                                         int type,
                                                         int count)
{
    NativeProcessProtocolSP process_sp (m_thread.GetProcess());
    if (!process_sp)
        return Error("NativeProcessProtocol is NULL");
    NativeProcessLinux *const process_p = reinterpret_cast<NativeProcessLinux*>(process_sp.get());

    return process_p->DoOperation([&] {
        struct iovec ioVec;
        struct user_hwdebug_state dreg_state;
        Error error;

        memset (&dreg_state, 0, sizeof (dreg_state));
        ioVec.iov_base = &dreg_state;
        ioVec.iov_len = sizeof (dreg_state);

        if (type == 0)
            type = NT_ARM_HW_WATCH;
        else
            type = NT_ARM_HW_BREAK;

        for (int i = 0; i < count; i++)
        {
            dreg_state.dbg_regs[i].addr = addr_buf[i];
            dreg_state.dbg_regs[i].ctrl = cntrl_buf[i];
        }

        return NativeProcessLinux::PtraceWrapper(PTRACE_SETREGSET, m_thread.GetID(), &type, &ioVec, ioVec.iov_len);
    });
}

Error
NativeRegisterContextLinux_arm64::DoReadRegisterValue(uint32_t offset,
                                                      const char* reg_name,
                                                      uint32_t size,
                                                      RegisterValue &value)
{
    Error error;
    if (offset > sizeof(struct user_pt_regs))
    {
        uintptr_t offset = offset - sizeof(struct user_pt_regs);
        if (offset > sizeof(struct user_fpsimd_state))
        {
            error.SetErrorString("invalid offset value");
            return error;
        }
        elf_fpregset_t regs;
        int regset = NT_FPREGSET;
        struct iovec ioVec;

        ioVec.iov_base = &regs;
        ioVec.iov_len = sizeof regs;
        error = NativeProcessLinux::PtraceWrapper(
                PTRACE_GETREGSET, m_thread.GetID(), &regset, &ioVec, sizeof regs);
        if (error.Success())
        {
            ArchSpec arch;
            if (m_thread.GetProcess()->GetArchitecture(arch))
                value.SetBytes((void *)(((unsigned char *)(&regs)) + offset), 16, arch.GetByteOrder());
            else
                error.SetErrorString("failed to get architecture");
        }
    }
    else
    {
        elf_gregset_t regs;
        int regset = NT_PRSTATUS;
        struct iovec ioVec;

        ioVec.iov_base = &regs;
        ioVec.iov_len = sizeof regs;
        error = NativeProcessLinux::PtraceWrapper(
                PTRACE_GETREGSET, m_thread.GetID(), &regset, &ioVec, sizeof regs);
        if (error.Success())
        {
            ArchSpec arch;
            if (m_thread.GetProcess()->GetArchitecture(arch))
                value.SetBytes((void *)(((unsigned char *)(regs)) + offset), 8, arch.GetByteOrder());
            else
                error.SetErrorString("failed to get architecture");
        }
    }
    return error;
}

Error
NativeRegisterContextLinux_arm64::DoWriteRegisterValue(uint32_t offset,
                                                       const char* reg_name,
                                                       const RegisterValue &value)
{
    Error error;
    ::pid_t tid = m_thread.GetID();
    if (offset > sizeof(struct user_pt_regs))
    {
        uintptr_t offset = offset - sizeof(struct user_pt_regs);
        if (offset > sizeof(struct user_fpsimd_state))
        {
            error.SetErrorString("invalid offset value");
            return error;
        }
        elf_fpregset_t regs;
        int regset = NT_FPREGSET;
        struct iovec ioVec;

        ioVec.iov_base = &regs;
        ioVec.iov_len = sizeof regs;
        error = NativeProcessLinux::PtraceWrapper( PTRACE_GETREGSET, tid, &regset, &ioVec, sizeof regs);

        if (error.Success())
        {
            ::memcpy((void *)(((unsigned char *)(&regs)) + offset), value.GetBytes(), 16);
            error = NativeProcessLinux::PtraceWrapper(PTRACE_SETREGSET, tid, &regset, &ioVec, sizeof regs);
        }
    }
    else
    {
        elf_gregset_t regs;
        int regset = NT_PRSTATUS;
        struct iovec ioVec;

        ioVec.iov_base = &regs;
        ioVec.iov_len = sizeof regs;
        error = NativeProcessLinux::PtraceWrapper(PTRACE_GETREGSET, tid, &regset, &ioVec, sizeof regs);
        if (error.Success())
        {
            ::memcpy((void *)(((unsigned char *)(&regs)) + offset), value.GetBytes(), 8);
            error = NativeProcessLinux::PtraceWrapper(PTRACE_SETREGSET, tid, &regset, &ioVec, sizeof regs);
        }
    }
    return error;
}

Error
NativeRegisterContextLinux_arm64::DoReadGPR(void *buf, size_t buf_size)
{
    int regset = NT_PRSTATUS;
    struct iovec ioVec;
    Error error;

    ioVec.iov_base = buf;
    ioVec.iov_len = buf_size;
    return NativeProcessLinux::PtraceWrapper(PTRACE_GETREGSET, m_thread.GetID(), &regset, &ioVec, buf_size);
}

Error
NativeRegisterContextLinux_arm64::DoWriteGPR(void *buf, size_t buf_size)
{
    int regset = NT_PRSTATUS;
    struct iovec ioVec;
    Error error;

    ioVec.iov_base = buf;
    ioVec.iov_len = buf_size;
    return NativeProcessLinux::PtraceWrapper(PTRACE_SETREGSET, m_thread.GetID(), &regset, &ioVec, buf_size);
}

Error
NativeRegisterContextLinux_arm64::DoReadFPR(void *buf, size_t buf_size)
{
    int regset = NT_FPREGSET;
    struct iovec ioVec;
    Error error;

    ioVec.iov_base = buf;
    ioVec.iov_len = buf_size;
    return NativeProcessLinux::PtraceWrapper(PTRACE_GETREGSET, m_thread.GetID(), &regset, &ioVec, buf_size);
}

Error
NativeRegisterContextLinux_arm64::DoWriteFPR(void *buf, size_t buf_size)
{
    int regset = NT_FPREGSET;
    struct iovec ioVec;
    Error error;

    ioVec.iov_base = buf;
    ioVec.iov_len = buf_size;
    return NativeProcessLinux::PtraceWrapper(PTRACE_SETREGSET, m_thread.GetID(), &regset, &ioVec, buf_size);
}

#endif // defined (__arm64__) || defined (__aarch64__)
