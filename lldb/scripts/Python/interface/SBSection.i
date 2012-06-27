//===-- SWIG Interface for SBSection ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace lldb {

%feature("docstring",
"Represents an executable image section.

SBSection supports iteration through its subsection, represented as SBSection
as well.  For example,

    for sec in exe_module:
        if sec.GetName() == '__TEXT':
            print sec
            break
    print INDENT + 'Number of subsections: %d' % sec.GetNumSubSections()
    for subsec in sec:
        print INDENT + repr(subsec)

produces:

[0x0000000100000000-0x0000000100002000) a.out.__TEXT
    Number of subsections: 6
    [0x0000000100001780-0x0000000100001d5c) a.out.__TEXT.__text
    [0x0000000100001d5c-0x0000000100001da4) a.out.__TEXT.__stubs
    [0x0000000100001da4-0x0000000100001e2c) a.out.__TEXT.__stub_helper
    [0x0000000100001e2c-0x0000000100001f10) a.out.__TEXT.__cstring
    [0x0000000100001f10-0x0000000100001f68) a.out.__TEXT.__unwind_info
    [0x0000000100001f68-0x0000000100001ff8) a.out.__TEXT.__eh_frame

See also SBModule."
) SBSection;

class SBSection
{
public:

    SBSection ();

    SBSection (const lldb::SBSection &rhs);

    ~SBSection ();

    bool
    IsValid () const;

    const char *
    GetName ();

    lldb::SBSection
    FindSubSection (const char *sect_name);

    size_t
    GetNumSubSections ();

    lldb::SBSection
    GetSubSectionAtIndex (size_t idx);

    lldb::addr_t
    GetFileAddress ();

    lldb::addr_t
    GetLoadAddress (lldb::SBTarget &target);
    
    lldb::addr_t
    GetByteSize ();

    uint64_t
    GetFileOffset ();

    uint64_t
    GetFileByteSize ();
    
    lldb::SBData
    GetSectionData ();

    lldb::SBData
    GetSectionData (uint64_t offset,
                    uint64_t size);

    SectionType
    GetSectionType ();

    bool
    GetDescription (lldb::SBStream &description);
    
    %pythoncode %{
        def get_addr(self):
            return SBAddress(self, 0)

        __swig_getmethods__["name"] = GetName
        if _newclass: x = property(GetName, None)

        __swig_getmethods__["addr"] = get_addr
        if _newclass: x = property(get_addr, None)

        __swig_getmethods__["file_addr"] = GetFileAddress
        if _newclass: x = property(GetFileAddress, None)

        __swig_getmethods__["size"] = GetByteSize
        if _newclass: x = property(GetByteSize, None)

        __swig_getmethods__["file_offset"] = GetFileOffset
        if _newclass: x = property(GetFileOffset, None)

        __swig_getmethods__["file_size"] = GetFileByteSize
        if _newclass: x = property(GetFileByteSize, None)

        __swig_getmethods__["data"] = GetSectionData
        if _newclass: x = property(GetSectionData, None)

        __swig_getmethods__["type"] = GetSectionType
        if _newclass: x = property(GetSectionType, None)

    %}

private:

    std::auto_ptr<lldb_private::SectionImpl> m_opaque_ap;
};

} // namespace lldb
