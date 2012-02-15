//===-- SWIG Interface for SBTypeNameSpecifier---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace lldb {
    
    %feature("docstring",
    "Represents a general way to provide a type name to LLDB APIs.
    ") SBTypeNameSpecifier;
    
    class SBTypeNameSpecifier
    {
    public:
        
        SBTypeNameSpecifier();
        
        SBTypeNameSpecifier (const char* name,
                             bool is_regex = false);
        
        SBTypeNameSpecifier (const lldb::SBTypeNameSpecifier &rhs);
        
        ~SBTypeNameSpecifier ();
        
        bool
        IsValid() const;
        
        bool
        IsEqualTo (lldb::SBTypeNameSpecifier &rhs);
        
        const char*
        GetName();
        
        bool
        IsRegex();
        
        bool
        GetDescription (lldb::SBStream &description, 
                        lldb::DescriptionLevel description_level);
                        
        %pythoncode %{
            __swig_getmethods__["name"] = GetName
            if _newclass: x = property(GetName, None)
            
            __swig_getmethods__["is_regex"] = IsRegex
            if _newclass: x = property(IsRegex, None)
        %}

        
    };
    
} // namespace lldb

