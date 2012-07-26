//===-- ExecutionContext.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/ExecutionContextScope.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"

using namespace lldb_private;

ExecutionContext::ExecutionContext() :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
}

ExecutionContext::ExecutionContext (const ExecutionContext &rhs) :
    m_target_sp(rhs.m_target_sp),
    m_process_sp(rhs.m_process_sp),
    m_thread_sp(rhs.m_thread_sp),
    m_frame_sp(rhs.m_frame_sp)
{
}

ExecutionContext::ExecutionContext (const lldb::TargetSP &target_sp, bool get_process) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (target_sp)
        SetContext (target_sp, get_process);
}

ExecutionContext::ExecutionContext (const lldb::ProcessSP &process_sp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (process_sp)
        SetContext (process_sp);
}

ExecutionContext::ExecutionContext (const lldb::ThreadSP &thread_sp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (thread_sp)
        SetContext (thread_sp);
}

ExecutionContext::ExecutionContext (const lldb::StackFrameSP &frame_sp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (frame_sp)
        SetContext (frame_sp);
}

ExecutionContext::ExecutionContext (const lldb::TargetWP &target_wp, bool get_process) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    lldb::TargetSP target_sp(target_wp.lock());
    if (target_sp)
        SetContext (target_sp, get_process);
}

ExecutionContext::ExecutionContext (const lldb::ProcessWP &process_wp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    lldb::ProcessSP process_sp(process_wp.lock());
    if (process_sp)
        SetContext (process_sp);
}

ExecutionContext::ExecutionContext (const lldb::ThreadWP &thread_wp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    lldb::ThreadSP thread_sp(thread_wp.lock());
    if (thread_sp)
        SetContext (thread_sp);
}

ExecutionContext::ExecutionContext (const lldb::StackFrameWP &frame_wp) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    lldb::StackFrameSP frame_sp(frame_wp.lock());
    if (frame_sp)
        SetContext (frame_sp);
}

ExecutionContext::ExecutionContext (Target* t, bool fill_current_process_thread_frame) :
    m_target_sp (t->shared_from_this()),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (t && fill_current_process_thread_frame)
    {
        m_process_sp = t->GetProcessSP();
        if (m_process_sp)
        {
            m_thread_sp = m_process_sp->GetThreadList().GetSelectedThread();
            if (m_thread_sp)
                m_frame_sp = m_thread_sp->GetSelectedFrame();
        }
    }
}

ExecutionContext::ExecutionContext(Process* process, Thread *thread, StackFrame *frame) :
    m_target_sp (),
    m_process_sp (process->shared_from_this()),
    m_thread_sp (thread->shared_from_this()),
    m_frame_sp (frame->shared_from_this())
{
    if (process)
        m_target_sp = process->GetTarget().shared_from_this();
}

ExecutionContext::ExecutionContext (const ExecutionContextRef &exe_ctx_ref) :
    m_target_sp (exe_ctx_ref.GetTargetSP()),
    m_process_sp (exe_ctx_ref.GetProcessSP()),
    m_thread_sp (exe_ctx_ref.GetThreadSP()),
    m_frame_sp (exe_ctx_ref.GetFrameSP())
{
}

ExecutionContext::ExecutionContext (const ExecutionContextRef *exe_ctx_ref_ptr) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (exe_ctx_ref_ptr)
    {
        m_target_sp  = exe_ctx_ref_ptr->GetTargetSP();
        m_process_sp = exe_ctx_ref_ptr->GetProcessSP();
        m_thread_sp = exe_ctx_ref_ptr->GetThreadSP();
        m_frame_sp = exe_ctx_ref_ptr->GetFrameSP();
    }
}

ExecutionContext::ExecutionContext (ExecutionContextScope *exe_scope_ptr) :
    m_target_sp (),
    m_process_sp (),
    m_thread_sp (),
    m_frame_sp ()
{
    if (exe_scope_ptr)
        exe_scope_ptr->CalculateExecutionContext (*this);
}

ExecutionContext::ExecutionContext (ExecutionContextScope &exe_scope_ref)
{
    exe_scope_ref.CalculateExecutionContext (*this);
}

void
ExecutionContext::Clear()
{
    m_target_sp.reset();
    m_process_sp.reset();
    m_thread_sp.reset();
    m_frame_sp.reset();
}

ExecutionContext::~ExecutionContext()
{
}

uint32_t
ExecutionContext::GetAddressByteSize() const
{
    if (m_target_sp && m_target_sp->GetArchitecture().IsValid())
        m_target_sp->GetArchitecture().GetAddressByteSize();
    if (m_process_sp)
        m_process_sp->GetAddressByteSize();
    return sizeof(void *);
}



RegisterContext *
ExecutionContext::GetRegisterContext () const
{
    if (m_frame_sp)
        return m_frame_sp->GetRegisterContext().get();
    else if (m_thread_sp)
        return m_thread_sp->GetRegisterContext().get();
    return NULL;
}

Target *
ExecutionContext::GetTargetPtr () const
{
    if (m_target_sp)
        return m_target_sp.get();
    if (m_process_sp)
        return &m_process_sp->GetTarget();
    return NULL;
}

Process *
ExecutionContext::GetProcessPtr () const
{
    if (m_process_sp)
        return m_process_sp.get();
    if (m_target_sp)
        return m_target_sp->GetProcessSP().get();
    return NULL;
}

ExecutionContextScope *
ExecutionContext::GetBestExecutionContextScope () const
{
    if (m_frame_sp)
        return m_frame_sp.get();
    if (m_thread_sp)
        return m_thread_sp.get();
    if (m_process_sp)
        return m_process_sp.get();
    return m_target_sp.get();
}

Target &
ExecutionContext::GetTargetRef () const
{
#if defined (LLDB_CONFIGURATION_DEBUG) || defined (LLDB_CONFIGURATION_RELEASE)
    assert (m_target_sp.get());
#endif
    return *m_target_sp;
}

Process &
ExecutionContext::GetProcessRef () const
{
#if defined (LLDB_CONFIGURATION_DEBUG) || defined (LLDB_CONFIGURATION_RELEASE)
    assert (m_process_sp.get());
#endif
    return *m_process_sp;
}

Thread &
ExecutionContext::GetThreadRef () const
{
#if defined (LLDB_CONFIGURATION_DEBUG) || defined (LLDB_CONFIGURATION_RELEASE)
    assert (m_thread_sp.get());
#endif
    return *m_thread_sp;
}

StackFrame &
ExecutionContext::GetFrameRef () const
{
#if defined (LLDB_CONFIGURATION_DEBUG) || defined (LLDB_CONFIGURATION_RELEASE)
    assert (m_frame_sp.get());
#endif
    return *m_frame_sp;
}

void
ExecutionContext::SetTargetSP (const lldb::TargetSP &target_sp)
{
    m_target_sp = target_sp;
}

void
ExecutionContext::SetProcessSP (const lldb::ProcessSP &process_sp)
{
    m_process_sp = process_sp;
}

void
ExecutionContext::SetThreadSP (const lldb::ThreadSP &thread_sp)
{
    m_thread_sp = thread_sp;
}

void
ExecutionContext::SetFrameSP (const lldb::StackFrameSP &frame_sp)
{
    m_frame_sp = frame_sp;
}

void
ExecutionContext::SetTargetPtr (Target* target)
{
    if (target)
        m_target_sp = target->shared_from_this();
    else
        m_target_sp.reset();
}

void
ExecutionContext::SetProcessPtr (Process *process)
{
    if (process)
        m_process_sp = process->shared_from_this();
    else
        m_process_sp.reset();
}

void
ExecutionContext::SetThreadPtr (Thread *thread)
{
    if (thread)
        m_thread_sp = thread->shared_from_this();
    else
        m_thread_sp.reset();
}

void
ExecutionContext::SetFramePtr (StackFrame *frame)
{
    if (frame)
        m_frame_sp = frame->shared_from_this();
    else
        m_frame_sp.reset();
}

void
ExecutionContext::SetContext (const lldb::TargetSP &target_sp, bool get_process)
{
    m_target_sp = target_sp;
    if (get_process && target_sp)
        m_process_sp = target_sp->GetProcessSP();
    else
        m_process_sp.reset();
    m_thread_sp.reset();
    m_frame_sp.reset();
}

void
ExecutionContext::SetContext (const lldb::ProcessSP &process_sp)
{
    m_process_sp = process_sp;
    if (process_sp)
        m_target_sp = process_sp->GetTarget().shared_from_this();
    else
        m_target_sp.reset();
    m_thread_sp.reset();
    m_frame_sp.reset();
}

void
ExecutionContext::SetContext (const lldb::ThreadSP &thread_sp)
{
    m_frame_sp.reset();
    m_thread_sp = thread_sp;
    if (thread_sp)
    {
        m_process_sp = thread_sp->GetProcess();
        if (m_process_sp)
            m_target_sp = m_process_sp->GetTarget().shared_from_this();
        else
            m_target_sp.reset();
    }
    else
    {
        m_target_sp.reset();
        m_process_sp.reset();
    }
}

void
ExecutionContext::SetContext (const lldb::StackFrameSP &frame_sp)
{
    m_frame_sp = frame_sp;
    if (frame_sp)
    {
        m_thread_sp = frame_sp->CalculateThread();
        if (m_thread_sp)
        {
            m_process_sp = m_thread_sp->GetProcess();
            if (m_process_sp)
                m_target_sp = m_process_sp->GetTarget().shared_from_this();
            else
                m_target_sp.reset();
        }
        else
        {
            m_target_sp.reset();
            m_process_sp.reset();
        }
    }
    else
    {
        m_target_sp.reset();
        m_process_sp.reset();
        m_thread_sp.reset();
    }
}

ExecutionContext &
ExecutionContext::operator =(const ExecutionContext &rhs)
{
    if (this != &rhs)
    {
        m_target_sp  = rhs.m_target_sp;
        m_process_sp = rhs.m_process_sp;
        m_thread_sp  = rhs.m_thread_sp;
        m_frame_sp   = rhs.m_frame_sp;
    }
    return *this;
}

bool
ExecutionContext::operator ==(const ExecutionContext &rhs) const
{
    // Check that the frame shared pointers match, or both are valid and their stack
    // IDs match since sometimes we get new objects that represent the same 
    // frame within a thread.
    if ((m_frame_sp == rhs.m_frame_sp) || (m_frame_sp && rhs.m_frame_sp && m_frame_sp->GetStackID() == rhs.m_frame_sp->GetStackID()))
    {
        // Check that the thread shared pointers match, or both are valid and 
        // their thread IDs match since sometimes we get new objects that 
        // represent the same thread within a process.
        if ((m_thread_sp == rhs.m_thread_sp) || (m_thread_sp && rhs.m_thread_sp && m_thread_sp->GetID() == rhs.m_thread_sp->GetID()))
        {
            // Processes and targets don't change much
            return m_process_sp == rhs.m_process_sp && m_target_sp == rhs.m_target_sp;
        }
    }
    return false;
}

bool
ExecutionContext::operator !=(const ExecutionContext &rhs) const
{
    return !(*this == rhs);
}


ExecutionContextRef::ExecutionContextRef() :
    m_target_wp (),
    m_process_wp (),
    m_thread_wp (),
    m_frame_wp (),
    m_tid(LLDB_INVALID_THREAD_ID),  
    m_stack_id ()
{
}

ExecutionContextRef::ExecutionContextRef (const ExecutionContext *exe_ctx) :
    m_target_wp (),
    m_process_wp (),
    m_thread_wp (),
    m_frame_wp (),
    m_tid(LLDB_INVALID_THREAD_ID),  
    m_stack_id ()
{
    if (exe_ctx)
        *this = *exe_ctx;
}

ExecutionContextRef::ExecutionContextRef (const ExecutionContext &exe_ctx) :
    m_target_wp (),
    m_process_wp (),
    m_thread_wp (),
    m_frame_wp (),
    m_tid(LLDB_INVALID_THREAD_ID),  
    m_stack_id ()
{
    *this = exe_ctx;
}


ExecutionContextRef::ExecutionContextRef (Target *target, bool adopt_selected) :
    m_target_wp(),
    m_process_wp(),
    m_thread_wp(),
    m_frame_wp(),
    m_tid(LLDB_INVALID_THREAD_ID),  
    m_stack_id ()
{
    SetTargetPtr (target, adopt_selected);
}




ExecutionContextRef::ExecutionContextRef (const ExecutionContextRef &rhs) :
    m_target_wp (rhs.m_target_wp),
    m_process_wp(rhs.m_process_wp),
    m_thread_wp (rhs.m_thread_wp),
    m_frame_wp  (rhs.m_frame_wp),
    m_tid       (rhs.m_tid),
    m_stack_id  (rhs.m_stack_id)
{
}

ExecutionContextRef &
ExecutionContextRef::operator =(const ExecutionContextRef &rhs)
{
    if (this != &rhs)
    {
        m_target_wp  = rhs.m_target_wp;
        m_process_wp = rhs.m_process_wp;
        m_thread_wp  = rhs.m_thread_wp;
        m_frame_wp   = rhs.m_frame_wp;
        m_tid        = rhs.m_tid;
        m_stack_id   = rhs.m_stack_id;
    }
    return *this;
}

ExecutionContextRef &
ExecutionContextRef::operator =(const ExecutionContext &exe_ctx)
{
    m_target_wp = exe_ctx.GetTargetSP();
    m_process_wp = exe_ctx.GetProcessSP();
    lldb::ThreadSP thread_sp (exe_ctx.GetThreadSP());
    m_thread_wp = thread_sp;
    if (thread_sp)
        m_tid = thread_sp->GetID();
    else
        m_tid = LLDB_INVALID_THREAD_ID;
    lldb::StackFrameSP frame_sp (exe_ctx.GetFrameSP());
    m_frame_wp = frame_sp;
    if (frame_sp)
        m_stack_id = frame_sp->GetStackID();
    else
        m_stack_id.Clear();
    return *this;
}

void
ExecutionContextRef::Clear()
{
    m_target_wp.reset();
    m_process_wp.reset();
    ClearThread();
    ClearFrame();
}

ExecutionContextRef::~ExecutionContextRef()
{
}

void
ExecutionContextRef::SetTargetSP (const lldb::TargetSP &target_sp)
{
    m_target_wp = target_sp;
}

void
ExecutionContextRef::SetProcessSP (const lldb::ProcessSP &process_sp)
{
    if (process_sp)
    {
        m_process_wp = process_sp;
        SetTargetSP (process_sp->GetTarget().shared_from_this());
    }
    else
    {
        m_process_wp.reset();
        m_target_wp.reset();
    }
}

void
ExecutionContextRef::SetThreadSP (const lldb::ThreadSP &thread_sp)
{
    if (thread_sp)
    {
        m_thread_wp = thread_sp;
        m_tid = thread_sp->GetID();
        SetProcessSP (thread_sp->GetProcess());
    }
    else
    {
        ClearThread();
        m_process_wp.reset();
        m_target_wp.reset();
    }
}

void
ExecutionContextRef::SetFrameSP (const lldb::StackFrameSP &frame_sp)
{
    if (frame_sp)
    {
        m_frame_wp = frame_sp;
        m_stack_id = frame_sp->GetStackID();
        SetThreadSP (frame_sp->GetThread());
    }
    else
    {
        ClearFrame();
        ClearThread();
        m_process_wp.reset();
        m_target_wp.reset();
    }

}

void
ExecutionContextRef::SetTargetPtr (Target* target, bool adopt_selected)
{
    Clear();
    if (target)
    {
        lldb::TargetSP target_sp (target->shared_from_this());
        if (target_sp)
        {
            m_target_wp = target_sp;
            if (adopt_selected)
            {
                lldb::ProcessSP process_sp (target_sp->GetProcessSP());
                if (process_sp)
                {
                    m_process_wp = process_sp;
                    if (process_sp)
                    {
                        lldb::ThreadSP thread_sp (process_sp->GetThreadList().GetSelectedThread());
                        if (!thread_sp)
                            thread_sp = process_sp->GetThreadList().GetThreadAtIndex(0);
                        
                        if (thread_sp && process_sp->GetState() == lldb::eStateStopped)
                        {
                            SetThreadSP (thread_sp);
                            lldb::StackFrameSP frame_sp (thread_sp->GetSelectedFrame());
                            if (!frame_sp)
                                frame_sp = thread_sp->GetStackFrameAtIndex(0);
                            if (frame_sp)
                                SetFrameSP (frame_sp);
                        }
                    }
                }
            }
        }
    }
}

void
ExecutionContextRef::SetProcessPtr (Process *process)
{
    if (process)
    {
        SetProcessSP(process->shared_from_this());
    }
    else
    {
        m_process_wp.reset();
        m_target_wp.reset();
    }
}

void
ExecutionContextRef::SetThreadPtr (Thread *thread)
{
    if (thread)
    {
        SetThreadSP (thread->shared_from_this());
    }
    else
    {
        ClearThread();
        m_process_wp.reset();
        m_target_wp.reset();
    }
}

void
ExecutionContextRef::SetFramePtr (StackFrame *frame)
{
    if (frame)
        SetFrameSP (frame->shared_from_this());
    else
        Clear();
}


lldb::ThreadSP
ExecutionContextRef::GetThreadSP () const
{
    lldb::ThreadSP thread_sp (m_thread_wp.lock());
    if (m_tid != LLDB_INVALID_THREAD_ID)
    {
        // We check if the thread has been destroyed in cases where clients
        // might still have shared pointer to a thread, but the thread is
        // not valid anymore (not part of the process)
        if (!thread_sp || !thread_sp->IsValid())
        {
            lldb::ProcessSP process_sp(GetProcessSP());
            if (process_sp)
            {
                thread_sp = process_sp->GetThreadList().FindThreadByID(m_tid);
                m_thread_wp = thread_sp;
            }
        }
    }
    return thread_sp;
}

lldb::StackFrameSP
ExecutionContextRef::GetFrameSP () const
{
    lldb::StackFrameSP frame_sp (m_frame_wp.lock());
    if (!frame_sp && m_stack_id.IsValid())
    {
        lldb::ThreadSP thread_sp (GetThreadSP());
        if (thread_sp)
        {
            frame_sp = thread_sp->GetFrameWithStackID (m_stack_id);
            m_frame_wp = frame_sp;
        }
    }
    return frame_sp;
}

ExecutionContext
ExecutionContextRef::Lock () const
{
    return ExecutionContext(this);
}


