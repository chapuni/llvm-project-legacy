//===-- SBAddress.h ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SBAddress_h_
#define LLDB_SBAddress_h_

#include "lldb/API/SBDefines.h"

namespace lldb {

class SBAddress
{
public:

    SBAddress ();

    SBAddress (const lldb::SBAddress &rhs);

    ~SBAddress ();

#ifndef SWIG
    const SBAddress &
    operator = (const SBAddress &rhs);
#endif

    bool
    IsValid () const;

    void
    Clear ();

    addr_t
    GetFileAddress () const;

    addr_t
    GetLoadAddress (const lldb::SBTarget &target) const;

    bool
    OffsetAddress (addr_t offset);

    bool
    GetDescription (lldb::SBStream &description);

    // The following function gets called by Python when a user tries to print
    // an object of this class.  It takes no arguments and returns a
    // PyObject * representing a char * (and it must be named "__repr__");

    PyObject *
    __repr__ ();

protected:

    friend class SBFrame;
    friend class SBLineEntry;
    friend class SBModule;
    friend class SBSymbolContext;
    friend class SBThread;

#ifndef SWIG

    lldb_private::Address *
    operator->();

    const lldb_private::Address *
    operator->() const;

    const lldb_private::Address &
    operator*() const;

    lldb_private::Address &
    operator*();

#endif


    SBAddress (const lldb_private::Address *lldb_object_ptr);

    void
    SetAddress (const lldb_private::Address *lldb_object_ptr);

private:

    std::auto_ptr<lldb_private::Address> m_opaque_ap;
};


} // namespace lldb

#endif // LLDB_SBAddress_h_
