//===-- FormatManager.h -------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_FormatManager_h_
#define lldb_FormatManager_h_

// C Includes

#include <stdint.h>
#include <unistd.h>

// C++ Includes

#ifdef __GNUC__
#include <ext/hash_map>

namespace std
{
    using namespace __gnu_cxx;
}

#else
#include <hash_map>
#endif

#include <list>
#include <map>
#include <stack>

// Other libraries and framework includes
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/DeclObjC.h"

// Project includes
#include "lldb/lldb-public.h"
#include "lldb/lldb-enumerations.h"

#include "lldb/Core/Communication.h"
#include "lldb/Core/FormatClasses.h"
#include "lldb/Core/InputReaderStack.h"
#include "lldb/Core/Listener.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/RegularExpression.h"
#include "lldb/Core/StreamFile.h"
#include "lldb/Core/SourceManager.h"
#include "lldb/Core/UserID.h"
#include "lldb/Core/UserSettingsController.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Interpreter/ScriptInterpreterPython.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/TargetList.h"

using lldb::LogSP;

namespace lldb_private {
    
class IFormatChangeListener
{
public:
    virtual void
    Changed () = 0;
    
    virtual
    ~IFormatChangeListener () {}
    
    virtual uint32_t
    GetCurrentRevision () = 0;
    
};
    
template<typename KeyType, typename ValueType>
class FormatNavigator;

template<typename KeyType, typename ValueType>
class FormatMap
{
public:

    typedef typename ValueType::SharedPointer ValueSP;
    typedef std::map<KeyType, ValueSP> MapType;
    typedef typename MapType::iterator MapIterator;
    typedef bool(*CallbackType)(void*, KeyType, const ValueSP&);
    
    FormatMap(IFormatChangeListener* lst = NULL) :
    m_map(),
    m_map_mutex(Mutex::eMutexTypeRecursive),
    listener(lst)
    {
    }
    
    void
    Add(KeyType name,
        const ValueSP& entry)
    {
        if (listener)
            entry->m_my_revision = listener->GetCurrentRevision();
        else
            entry->m_my_revision = 0;

        Mutex::Locker(m_map_mutex);
        m_map[name] = entry;
        if (listener)
            listener->Changed();
    }
    
    bool
    Delete (KeyType name)
    {
        Mutex::Locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        m_map.erase(name);
        if (listener)
            listener->Changed();
        return true;
    }
    
    void
    Clear ()
    {
        Mutex::Locker(m_map_mutex);
        m_map.clear();
        if (listener)
            listener->Changed();
    }
    
    bool
    Get(KeyType name,
        ValueSP& entry)
    {
        Mutex::Locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        entry = iter->second;
        return true;
    }
    
    void
    LoopThrough (CallbackType callback, void* param)
    {
        if (callback)
        {
            Mutex::Locker(m_map_mutex);
            MapIterator pos, end = m_map.end();
            for (pos = m_map.begin(); pos != end; pos++)
            {
                KeyType type = pos->first;
                if (!callback(param, type, pos->second))
                    break;
            }
        }
    }
    
    uint32_t
    GetCount ()
    {
        return m_map.size();
    }
    
private:
    MapType m_map;    
    Mutex m_map_mutex;
    IFormatChangeListener* listener;
    
    MapType&
    map ()
    {
        return m_map;
    }
    
    Mutex&
    mutex ()
    {
        return m_map_mutex;
    }
    
    friend class FormatNavigator<KeyType, ValueType>;
    friend class FormatManager;
    
};
    
template<typename KeyType, typename ValueType>
class FormatNavigator
{
private:
    typedef FormatMap<KeyType,ValueType> BackEndType;
    
    BackEndType m_format_map;
    
    std::string m_name;
        
public:
    typedef typename BackEndType::MapType MapType;
    typedef typename MapType::iterator MapIterator;
    typedef typename MapType::key_type MapKeyType;
    typedef typename MapType::mapped_type MapValueType;
    typedef typename BackEndType::CallbackType CallbackType;
    
    typedef typename lldb::SharedPtr<FormatNavigator<KeyType, ValueType> >::Type SharedPointer;
    
    friend class FormatCategory;

    FormatNavigator(std::string name,
                    IFormatChangeListener* lst = NULL) :
    m_format_map(lst),
    m_name(name),
    m_id_cs(ConstString("id"))
    {
    }
    
    void
    Add (const MapKeyType &type, const MapValueType& entry)
    {
        m_format_map.Add(type,entry);
    }
    
    // using ConstString instead of MapKeyType is necessary here
    // to make the partial template specializations below work
    bool
    Delete (ConstString type)
    {
        return m_format_map.Delete(type);
    }
        
    bool
    Get(ValueObject& valobj,
        MapValueType& entry,
        lldb::DynamicValueType use_dynamic,
        uint32_t* why = NULL)
    {
        uint32_t value = lldb::eFormatterChoiceCriterionDirectChoice;
        clang::QualType type = clang::QualType::getFromOpaquePtr(valobj.GetClangType());
        bool ret = Get(valobj, type, entry, use_dynamic, value);
        if (ret)
            entry = MapValueType(entry);
        else
            entry = MapValueType();        
        if (why)
            *why = value;
        return ret;
    }
    
    void
    Clear ()
    {
        m_format_map.Clear();
    }
    
    void
    LoopThrough (CallbackType callback, void* param)
    {
        m_format_map.LoopThrough(callback,param);
    }
    
    uint32_t
    GetCount ()
    {
        return m_format_map.GetCount();
    }
        
private:
    
    DISALLOW_COPY_AND_ASSIGN(FormatNavigator);
    
    ConstString m_id_cs;
    
    // using ConstString instead of MapKeyType is necessary here
    // to make the partial template specializations below work
    bool
    Get (ConstString type, MapValueType& entry)
    {
        return m_format_map.Get(type, entry);
    }
    
    bool Get_ObjC(ValueObject& valobj,
             ObjCLanguageRuntime::ObjCISA isa,
             MapValueType& entry,
             uint32_t& reason)
    {
        LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));
        if (log)
            log->Printf("going to an Objective-C dynamic scanning");
        Process* process = valobj.GetUpdatePoint().GetProcessSP().get();
        ObjCLanguageRuntime* runtime = process->GetObjCLanguageRuntime();
        if (runtime == NULL)
        {
            if (log)
                log->Printf("no valid ObjC runtime, bailing out");
            return false;
        }
        if (runtime->IsValidISA(isa) == false)
        {
            if (log)
                log->Printf("invalid ISA, bailing out");
            return false;
        }
        ConstString name = runtime->GetActualTypeName(isa);
        if (log)
            log->Printf("looking for formatter for %s", name.GetCString());
        if (Get(name, entry))
        {
            if (log)
                log->Printf("direct match found, returning");
            return true;
        }
        if (log)
            log->Printf("no direct match");
        ObjCLanguageRuntime::ObjCISA parent = runtime->GetParentClass(isa);
        if (runtime->IsValidISA(parent) == false)
        {
            if (log)
                log->Printf("invalid parent ISA, bailing out");
            return false;
        }
        if (parent == isa)
        {
            if (log)
                log->Printf("parent-child loop, bailing out");
            return false;
        }
        if (Get_ObjC(valobj, parent, entry, reason))
        {
            reason |= lldb::eFormatterChoiceCriterionNavigatedBaseClasses;
            return true;
        }
        return false;
    }
    
    bool Get(ValueObject& valobj,
             clang::QualType type,
             MapValueType& entry,
             lldb::DynamicValueType use_dynamic,
             uint32_t& reason)
    {
        LogSP log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));
        if (type.isNull())
        {
            if (log)
                log->Printf("type is NULL, returning");
            return false;
        }
        // clang::QualType type = q_type.getUnqualifiedType();
        type.removeLocalConst(); type.removeLocalVolatile(); type.removeLocalRestrict();
        const clang::Type* typePtr = type.getTypePtrOrNull();
        if (!typePtr)
        {
            if (log)
                log->Printf("type is NULL, returning");
            return false;
        }
        ConstString typeName(ClangASTType::GetTypeNameForQualType(type).c_str());
        if (valobj.GetBitfieldBitSize() > 0)
        {
            // for bitfields, append size to the typename so one can custom format them
            StreamString sstring;
            sstring.Printf("%s:%d",typeName.AsCString(),valobj.GetBitfieldBitSize());
            ConstString bitfieldname = ConstString(sstring.GetData());
            if (log)
                log->Printf("appended bitfield info, final result is %s", bitfieldname.GetCString());
            if (Get(bitfieldname, entry))
            {
                if (log)
                    log->Printf("bitfield direct match found, returning");
                return true;
            }
            else
            {
                reason |= lldb::eFormatterChoiceCriterionStrippedBitField;
                if (log)
                    log->Printf("no bitfield direct match");
            }
        }
        if (log)
            log->Printf("trying to get %s for VO name %s of type %s",
                        m_name.c_str(),
                        valobj.GetName().AsCString(),
                        typeName.AsCString());
        if (Get(typeName, entry))
        {
            if (log)
                log->Printf("direct match found, returning");
            return true;
        }
        if (log)
            log->Printf("no direct match");
        // look for a "base type", whatever that means
        if (typePtr->isReferenceType())
        {
            if (log)
                log->Printf("stripping reference");
            if (Get(valobj,type.getNonReferenceType(),entry, use_dynamic, reason) && !entry->m_skip_references)
            {
                reason |= lldb::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        if (use_dynamic != lldb::eNoDynamicValues &&
            (/*strstr(typeName, "id") == typeName ||*/
             ClangASTType::GetMinimumLanguage(valobj.GetClangAST(), valobj.GetClangType()) == lldb::eLanguageTypeObjC))
        {
            if (log)
                log->Printf("this is an ObjC 'id', let's do dynamic search");
            Process* process = valobj.GetUpdatePoint().GetProcessSP().get();
            ObjCLanguageRuntime* runtime = process->GetObjCLanguageRuntime();
            if (runtime == NULL)
            {
                if (log)
                    log->Printf("no valid ObjC runtime, skipping dynamic");
            }
            else
            {
                if (Get_ObjC(valobj, runtime->GetISA(valobj), entry, reason))
                {
                    reason |= lldb::eFormatterChoiceCriterionDynamicObjCHierarchy;
                    return true;
                }
            }
        }
        else if (use_dynamic != lldb::eNoDynamicValues && log)
        {
            log->Printf("typename: %s, typePtr = %p, id = %p",
                        typeName.AsCString(), typePtr, valobj.GetClangAST()->ObjCBuiltinIdTy.getTypePtr());
        }
        else if (log)
        {
            log->Printf("no dynamic");
        }
        if (typePtr->isPointerType())
        {
            if (log)
                log->Printf("stripping pointer");
            clang::QualType pointee = typePtr->getPointeeType();
            if (Get(valobj, pointee, entry, use_dynamic, reason) && !entry->m_skip_pointers)
            {
                reason |= lldb::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        if (typePtr->isObjCObjectPointerType())
        {
            if (use_dynamic != lldb::eNoDynamicValues &&
                typeName == m_id_cs)
            {
                if (log)
                    log->Printf("this is an ObjC 'id', let's do dynamic search");
                Process* process = valobj.GetUpdatePoint().GetProcessSP().get();
                ObjCLanguageRuntime* runtime = process->GetObjCLanguageRuntime();
                if (runtime == NULL)
                {
                    if (log)
                        log->Printf("no valid ObjC runtime, skipping dynamic");
                }
                else
                {
                    if (Get_ObjC(valobj, runtime->GetISA(valobj), entry, reason))
                    {
                        reason |= lldb::eFormatterChoiceCriterionDynamicObjCHierarchy;
                        return true;
                    }
                }
            }
            if (log)
                log->Printf("stripping ObjC pointer");
            
            // For some reason, C++ can quite easily obtain the type hierarchy for a ValueObject
            // even if the VO represent a pointer-to-class, as long as the typePtr is right
            // Objective-C on the other hand cannot really complete an @interface when
            // the VO refers to a pointer-to-@interface

            Error error;
            ValueObject* target = valobj.Dereference(error).get();
            if (error.Fail() || !target)
                return false;
            if (Get(*target, typePtr->getPointeeType(), entry, use_dynamic, reason) && !entry->m_skip_pointers)
            {
                reason |= lldb::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        const clang::ObjCObjectType *objc_class_type = typePtr->getAs<clang::ObjCObjectType>();
        if (objc_class_type)
        {
            if (log)
                log->Printf("working with ObjC");
            clang::ASTContext *ast = valobj.GetClangAST();
            if (ClangASTContext::GetCompleteType(ast, valobj.GetClangType()) && !objc_class_type->isObjCId())
            {
                clang::ObjCInterfaceDecl *class_interface_decl = objc_class_type->getInterface();
                if (class_interface_decl)
                {
                    if (log)
                        log->Printf("got an ObjCInterfaceDecl");
                    clang::ObjCInterfaceDecl *superclass_interface_decl = class_interface_decl->getSuperClass();
                    if (superclass_interface_decl)
                    {
                        if (log)
                            log->Printf("got a parent class for this ObjC class");
                        clang::QualType ivar_qual_type(ast->getObjCInterfaceType(superclass_interface_decl));
                        if (Get(valobj, ivar_qual_type, entry, use_dynamic, reason) && entry->m_cascades)
                        {
                            reason |= lldb::eFormatterChoiceCriterionNavigatedBaseClasses;
                            return true;
                        }
                    }
                }
            }
        }
        // for C++ classes, navigate up the hierarchy
        if (typePtr->isRecordType())
        {
            if (log)
                log->Printf("working with C++");
            clang::CXXRecordDecl* record = typePtr->getAsCXXRecordDecl();
            if (record)
            {
                if (!record->hasDefinition())
                    ClangASTContext::GetCompleteType(valobj.GetClangAST(), valobj.GetClangType());
                if (record->hasDefinition())
                {
                    clang::CXXRecordDecl::base_class_iterator pos,end;
                    if (record->getNumBases() > 0)
                    {
                        if (log)
                            log->Printf("look into bases");
                        end = record->bases_end();
                        for (pos = record->bases_begin(); pos != end; pos++)
                        {
                            if ((Get(valobj, pos->getType(), entry, use_dynamic, reason)) && entry->m_cascades)
                            {
                                reason |= lldb::eFormatterChoiceCriterionNavigatedBaseClasses;
                                return true;
                            }
                        }
                    }
                    if (record->getNumVBases() > 0)
                    {
                        if (log)
                            log->Printf("look into VBases");
                        end = record->vbases_end();
                        for (pos = record->vbases_begin(); pos != end; pos++)
                        {
                            if ((Get(valobj, pos->getType(), entry, use_dynamic, reason)) && entry->m_cascades)
                            {
                                reason |= lldb::eFormatterChoiceCriterionNavigatedBaseClasses;
                                return true;
                            }
                        }
                    }
                }
            }
        }
        // try to strip typedef chains
        const clang::TypedefType* type_tdef = type->getAs<clang::TypedefType>();
        if (type_tdef)
        {
            if (log)
                log->Printf("stripping typedef");
            if ((Get(valobj, type_tdef->getDecl()->getUnderlyingType(), entry, use_dynamic, reason)) && entry->m_cascades)
            {
                reason |= lldb::eFormatterChoiceCriterionNavigatedTypedefs;
                return true;
            }
        }
        return false;
    }
};

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SummaryFormat>::Get(ConstString key, lldb::SummaryFormatSP& value);

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SummaryFormat>::Delete(ConstString type);

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SyntheticFilter>::Get(ConstString key, SyntheticFilter::SharedPointer& value);

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SyntheticFilter>::Delete(ConstString type);

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SyntheticScriptProvider>::Get(ConstString key, SyntheticFilter::SharedPointer& value);

template<>
bool
FormatNavigator<lldb::RegularExpressionSP, SyntheticScriptProvider>::Delete(ConstString type);
    
class CategoryMap;
    
class FormatCategory
{
private:
    
    typedef FormatNavigator<ConstString, SummaryFormat> SummaryNavigator;
    typedef FormatNavigator<lldb::RegularExpressionSP, SummaryFormat> RegexSummaryNavigator;
    
    typedef FormatNavigator<ConstString, SyntheticFilter> FilterNavigator;
    typedef FormatNavigator<lldb::RegularExpressionSP, SyntheticFilter> RegexFilterNavigator;
    
    typedef FormatNavigator<ConstString, SyntheticScriptProvider> SynthNavigator;
    typedef FormatNavigator<lldb::RegularExpressionSP, SyntheticScriptProvider> RegexSynthNavigator;

    typedef SummaryNavigator::MapType SummaryMap;
    typedef RegexSummaryNavigator::MapType RegexSummaryMap;
    typedef FilterNavigator::MapType FilterMap;
    typedef RegexFilterNavigator::MapType RegexFilterMap;
    typedef SynthNavigator::MapType SynthMap;
    typedef RegexSynthNavigator::MapType RegexSynthMap;

public:
    
    enum FormatCategoryItem
    {
        eSummary =         0x0001,
        eRegexSummary =    0x1001,
        eFilter =          0x0002,
        eRegexFilter =     0x1002,
        eSynth =           0x0004,
        eRegexSynth =      0x1004,
    };
    
    typedef uint16_t FormatCategoryItems;
    static const uint16_t ALL_ITEM_TYPES = 0xFFFF;
    
    typedef SummaryNavigator::SharedPointer SummaryNavigatorSP;
    typedef RegexSummaryNavigator::SharedPointer RegexSummaryNavigatorSP;
    typedef FilterNavigator::SharedPointer FilterNavigatorSP;
    typedef RegexFilterNavigator::SharedPointer RegexFilterNavigatorSP;
    typedef SynthNavigator::SharedPointer SynthNavigatorSP;
    typedef RegexSynthNavigator::SharedPointer RegexSynthNavigatorSP;

    FormatCategory(IFormatChangeListener* clist,
                   std::string name);
    
    SummaryNavigatorSP
    GetSummaryNavigator ()
    {
        return SummaryNavigatorSP(m_summary_nav);
    }
    
    RegexSummaryNavigatorSP
    GetRegexSummaryNavigator ()
    {
        return RegexSummaryNavigatorSP(m_regex_summary_nav);
    }
    
    FilterNavigatorSP
    GetFilterNavigator ()
    {
        return FilterNavigatorSP(m_filter_nav);
    }
    
    RegexFilterNavigatorSP
    GetRegexFilterNavigator ()
    {
        return RegexFilterNavigatorSP(m_regex_filter_nav);
    }
    
    SynthNavigatorSP
    GetSyntheticNavigator ()
    {
        return SynthNavigatorSP(m_synth_nav);
    }
    
    RegexSynthNavigatorSP
    GetRegexSyntheticNavigator ()
    {
        return RegexSynthNavigatorSP(m_regex_synth_nav);
    }
    
    bool
    IsEnabled () const
    {
        return m_enabled;
    }
        
    bool
    Get(ValueObject& valobj,
        lldb::SummaryFormatSP& entry,
        lldb::DynamicValueType use_dynamic,
        uint32_t* reason = NULL)
    {
        if (!IsEnabled())
            return false;
        if (GetSummaryNavigator()->Get(valobj, entry, use_dynamic, reason))
            return true;
        bool regex = GetRegexSummaryNavigator()->Get(valobj, entry, use_dynamic, reason);
        if (regex && reason)
            *reason |= lldb::eFormatterChoiceCriterionRegularExpressionSummary;
        return regex;
    }
    
    bool
    Get(ValueObject& valobj,
        lldb::SyntheticChildrenSP& entry,
        lldb::DynamicValueType use_dynamic,
        uint32_t* reason = NULL);
    
    // just a shortcut for GetSummaryNavigator()->Clear; GetRegexSummaryNavigator()->Clear()
    void
    ClearSummaries ()
    {
        Clear(eSummary | eRegexSummary);
    }
    
    // just a shortcut for (GetSummaryNavigator()->Delete(name) || GetRegexSummaryNavigator()->Delete(name))
    bool
    DeleteSummaries (ConstString name)
    {
        return Delete(name, (eSummary | eRegexSummary));
    }
    
    
    void
    Clear (FormatCategoryItems items = ALL_ITEM_TYPES)
    {
        if ( (items & eSummary) == eSummary )
            m_summary_nav->Clear();
        if ( (items & eRegexSummary) == eRegexSummary )
            m_regex_summary_nav->Clear();
        if ( (items & eFilter)  == eFilter )
            m_filter_nav->Clear();
        if ( (items & eRegexFilter) == eRegexFilter )
            m_regex_filter_nav->Clear();
        if ( (items & eSynth)  == eSynth )
            m_synth_nav->Clear();
        if ( (items & eRegexSynth) == eRegexSynth )
            m_regex_synth_nav->Clear();
    }
    
    bool
    Delete(ConstString name,
           FormatCategoryItems items = ALL_ITEM_TYPES)
    {
        bool success = false;
        if ( (items & eSummary) == eSummary )
            success = m_summary_nav->Delete(name) || success;
        if ( (items & eRegexSummary) == eRegexSummary )
            success = m_regex_summary_nav->Delete(name) || success;
        if ( (items & eFilter)  == eFilter )
            success = m_filter_nav->Delete(name) || success;
        if ( (items & eRegexFilter) == eRegexFilter )
            success = m_regex_filter_nav->Delete(name) || success;
        if ( (items & eSynth)  == eSynth )
            success = m_synth_nav->Delete(name) || success;
        if ( (items & eRegexSynth) == eRegexSynth )
            success = m_regex_synth_nav->Delete(name) || success;
        return success;
    }
    
    uint32_t
    GetCount (FormatCategoryItems items = ALL_ITEM_TYPES)
    {
        uint32_t count = 0;
        if ( (items & eSummary) == eSummary )
            count += m_summary_nav->GetCount();
        if ( (items & eRegexSummary) == eRegexSummary )
            count += m_regex_summary_nav->GetCount();
        if ( (items & eFilter)  == eFilter )
            count += m_filter_nav->GetCount();
        if ( (items & eRegexFilter) == eRegexFilter )
            count += m_regex_filter_nav->GetCount();
        if ( (items & eSynth)  == eSynth )
            count += m_synth_nav->GetCount();
        if ( (items & eRegexSynth) == eRegexSynth )
            count += m_regex_synth_nav->GetCount();
        return count;
    }
    
    std::string
    GetName ()
    {
        return m_name;
    }
    
    bool
    AnyMatches(ConstString type_name,
               FormatCategoryItems items = ALL_ITEM_TYPES,
               bool only_enabled = true,
               const char** matching_category = NULL,
               FormatCategoryItems* matching_type = NULL);
    
    typedef lldb::SharedPtr<FormatCategory>::Type SharedPointer;
    
private:
    SummaryNavigator::SharedPointer m_summary_nav;
    RegexSummaryNavigator::SharedPointer m_regex_summary_nav;
    FilterNavigator::SharedPointer m_filter_nav;
    RegexFilterNavigator::SharedPointer m_regex_filter_nav;
    SynthNavigator::SharedPointer m_synth_nav;
    RegexSynthNavigator::SharedPointer m_regex_synth_nav;
    
    bool m_enabled;
    
    IFormatChangeListener* m_change_listener;
    
    Mutex m_mutex;
    
    std::string m_name;
    
    void
    Enable (bool value = true)
    {
        Mutex::Locker(m_mutex);
        m_enabled = value;        
        if (m_change_listener)
            m_change_listener->Changed();
    }
    
    void
    Disable ()
    {
        Enable(false);
    }
    
    friend class CategoryMap;
    
    friend class FormatNavigator<ConstString, SummaryFormat>;
    friend class FormatNavigator<lldb::RegularExpressionSP, SummaryFormat>;
    
    friend class FormatNavigator<ConstString, SyntheticFilter>;
    friend class FormatNavigator<lldb::RegularExpressionSP, SyntheticFilter>;
    
    friend class FormatNavigator<ConstString, SyntheticScriptProvider>;
    friend class FormatNavigator<lldb::RegularExpressionSP, SyntheticScriptProvider>;
    

};

class CategoryMap
{
private:
    typedef const char* KeyType;
    typedef FormatCategory ValueType;
    typedef ValueType::SharedPointer ValueSP;
    typedef std::list<lldb::FormatCategorySP> ActiveCategoriesList;
    typedef ActiveCategoriesList::iterator ActiveCategoriesIterator;
        
public:
    typedef std::map<KeyType, ValueSP> MapType;
    typedef MapType::iterator MapIterator;
    typedef bool(*CallbackType)(void*, KeyType, const ValueSP&);
    
    CategoryMap(IFormatChangeListener* lst = NULL) :
    m_map_mutex(Mutex::eMutexTypeRecursive),
    listener(lst),
    m_map(),
    m_active_categories()
    {
    }
    
    void
    Add(KeyType name,
        const ValueSP& entry)
    {
        Mutex::Locker(m_map_mutex);
        m_map[name] = entry;
        if (listener)
            listener->Changed();
    }
    
    bool
    Delete (KeyType name)
    {
        Mutex::Locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        m_map.erase(name);
        DisableCategory(name);
        if (listener)
            listener->Changed();
        return true;
    }
    
    void
    EnableCategory (KeyType category_name)
    {
        Mutex::Locker(m_map_mutex);
        ValueSP category;
        if (!Get(category_name,category))
            return;
        category->Enable();
        m_active_categories.push_front(category);
    }
    
    class delete_matching_categories
    {
        lldb::FormatCategorySP ptr;
    public:
        delete_matching_categories(lldb::FormatCategorySP p) : ptr(p)
        {}
        
        bool operator()(const lldb::FormatCategorySP& other)
        {
            return ptr.get() == other.get();
        }
    };
    
    void
    DisableCategory (KeyType category_name)
    {
        Mutex::Locker(m_map_mutex);
        ValueSP category;
        if (!Get(category_name,category))
            return;
        category->Disable();
        m_active_categories.remove_if(delete_matching_categories(category));
    }
    
    void
    Clear ()
    {
        Mutex::Locker(m_map_mutex);
        m_map.clear();
        m_active_categories.clear();
        if (listener)
            listener->Changed();
    }
    
    bool
    Get(KeyType name,
        ValueSP& entry)
    {
        Mutex::Locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        entry = iter->second;
        return true;
    }
    
    void
    LoopThrough (CallbackType callback, void* param);
    
    bool
    AnyMatches(ConstString type_name,
               FormatCategory::FormatCategoryItems items = FormatCategory::ALL_ITEM_TYPES,
               bool only_enabled = true,
               const char** matching_category = NULL,
               FormatCategory::FormatCategoryItems* matching_type = NULL)
    {
        Mutex::Locker(m_map_mutex);
        
        MapIterator pos, end = m_map.end();
        for (pos = m_map.begin(); pos != end; pos++)
        {
            if (pos->second->AnyMatches(type_name,
                                        items,
                                        only_enabled,
                                        matching_category,
                                        matching_type))
                return true;
        }
        return false;
    }
    
    uint32_t
    GetCount ()
    {
        return m_map.size();
    }
    
    bool
    Get(ValueObject& valobj,
        lldb::SummaryFormatSP& entry,
        lldb::DynamicValueType use_dynamic)
    {
        Mutex::Locker(m_map_mutex);
        
        uint32_t reason_why;        
        ActiveCategoriesIterator begin, end = m_active_categories.end();
        
        for (begin = m_active_categories.begin(); begin != end; begin++)
        {
            lldb::FormatCategorySP category = *begin;
            lldb::SummaryFormatSP current_format;
            if (!category->Get(valobj, current_format, use_dynamic, &reason_why))
                continue;
            entry = current_format;
            return true;
        }
        return false;
    }
    
    bool
    Get(ValueObject& valobj,
        lldb::SyntheticChildrenSP& entry,
        lldb::DynamicValueType use_dynamic)
    {
        Mutex::Locker(m_map_mutex);
        
        uint32_t reason_why;
        
        ActiveCategoriesIterator begin, end = m_active_categories.end();
        
        for (begin = m_active_categories.begin(); begin != end; begin++)
        {
            lldb::FormatCategorySP category = *begin;
            lldb::SyntheticChildrenSP current_format;
            if (!category->Get(valobj, current_format, use_dynamic, &reason_why))
                continue;
            entry = current_format;
            return true;
        }
        return false;
    }
    
private:
    Mutex m_map_mutex;
    IFormatChangeListener* listener;
    
    MapType m_map;
    ActiveCategoriesList m_active_categories;
    
    MapType& map ()
    {
        return m_map;
    }
    
    ActiveCategoriesList& active_list()
    {
        return m_active_categories;
    }
    
    Mutex& mutex ()
    {
        return m_map_mutex;
    }
    
    friend class FormatNavigator<KeyType, ValueType>;
    friend class FormatManager;
};


class FormatManager : public IFormatChangeListener
{
private:
    
    typedef FormatNavigator<ConstString, ValueFormat> ValueNavigator;

    typedef ValueNavigator::MapType ValueMap;
    typedef FormatMap<ConstString, SummaryFormat> NamedSummariesMap;
    typedef CategoryMap::MapType::iterator CategoryMapIterator;

public:
    
    typedef bool (*CategoryCallback)(void*, const char*, const lldb::FormatCategorySP&);
    
    FormatManager ();
    
    CategoryMap&
    Categories ()
    {
        return m_categories_map;
    }
    
    ValueNavigator&
    Value ()
    {
        return m_value_nav;
    }
    
    NamedSummariesMap&
    NamedSummary ()
    {
        return m_named_summaries_map;
    }
    
    void
    EnableCategory (const char* category_name)
    {
        m_categories_map.EnableCategory(category_name);
    }
    
    void
    DisableCategory (const char* category_name)
    {
        m_categories_map.DisableCategory(category_name);
    }
    
    void
    LoopThroughCategories (CategoryCallback callback, void* param)
    {
        m_categories_map.LoopThrough(callback, param);
    }
    
    FormatCategory::SummaryNavigatorSP
    Summary(const char* category_name = NULL)
    {
        return Category(category_name)->GetSummaryNavigator();
    }
    
    FormatCategory::RegexSummaryNavigatorSP
    RegexSummary (const char* category_name = NULL)
    {
        return Category(category_name)->GetRegexSummaryNavigator();
    }
    
    lldb::FormatCategorySP
    Category (const char* category_name = NULL)
    {
        if (!category_name)
            return Category(m_default_category_name);
        lldb::FormatCategorySP category;
        if (m_categories_map.Get(category_name, category))
            return category;
        Categories().Add(category_name,lldb::FormatCategorySP(new FormatCategory(this, category_name)));
        return Category(category_name);
    }
    
    bool
    Get(ValueObject& valobj,
        lldb::SummaryFormatSP& entry,
        lldb::DynamicValueType use_dynamic)
    {
        return m_categories_map.Get(valobj, entry, use_dynamic);
    }
    bool
    Get(ValueObject& valobj,
        lldb::SyntheticChildrenSP& entry,
        lldb::DynamicValueType use_dynamic)
    {
        return m_categories_map.Get(valobj, entry, use_dynamic);
    }
    
    bool
    AnyMatches(ConstString type_name,
               FormatCategory::FormatCategoryItems items = FormatCategory::ALL_ITEM_TYPES,
               bool only_enabled = true,
               const char** matching_category = NULL,
               FormatCategory::FormatCategoryItems* matching_type = NULL)
    {
        return m_categories_map.AnyMatches(type_name,
                                           items,
                                           only_enabled,
                                           matching_category,
                                           matching_type);
    }

    static bool
    GetFormatFromCString (const char *format_cstr,
                          bool partial_match_ok,
                          lldb::Format &format);

    static char
    GetFormatAsFormatChar (lldb::Format format);

    static const char *
    GetFormatAsCString (lldb::Format format);
    
    // when DataExtractor dumps a vectorOfT, it uses a predefined format for each item
    // this method returns it, or eFormatInvalid if vector_format is not a vectorOf
    static lldb::Format
    GetSingleItemFormat(lldb::Format vector_format);
    
    void
    Changed ()
    {
        __sync_add_and_fetch(&m_last_revision, +1);
    }
    
    uint32_t
    GetCurrentRevision ()
    {
        return m_last_revision;
    }
    
    ~FormatManager ()
    {
    }

private:    
    ValueNavigator m_value_nav;
    NamedSummariesMap m_named_summaries_map;
    uint32_t m_last_revision;
    CategoryMap m_categories_map;
    
    const char* m_default_category_name;
    const char* m_system_category_name;
    const char* m_gnu_cpp_category_name;
    
    ConstString m_default_cs;
    ConstString m_system_cs;
    ConstString m_gnu_stdcpp_cs;
};

class DataVisualization
{
public:
    
    // use this call to force the FM to consider itself updated even when there is no apparent reason for that
    static void
    ForceUpdate();
    
    static uint32_t
    GetCurrentRevision ();
    
    class ValueFormats
    {
    public:
        static bool
        Get (ValueObject& valobj, lldb::DynamicValueType use_dynamic, lldb::ValueFormatSP &entry);
        
        static void
        Add (const ConstString &type, const lldb::ValueFormatSP &entry);
        
        static bool
        Delete (const ConstString &type);
        
        static void
        Clear ();
        
        static void
        LoopThrough (ValueFormat::ValueCallback callback, void* callback_baton);
        
        static uint32_t
        GetCount ();
    };
    
    static bool
    GetSummaryFormat(ValueObject& valobj,
                     lldb::DynamicValueType use_dynamic,
                     lldb::SummaryFormatSP& entry);
    static bool
    GetSyntheticChildren(ValueObject& valobj,
                         lldb::DynamicValueType use_dynamic,
                         lldb::SyntheticChildrenSP& entry);
    
    static bool
    AnyMatches(ConstString type_name,
               FormatCategory::FormatCategoryItems items = FormatCategory::ALL_ITEM_TYPES,
               bool only_enabled = true,
               const char** matching_category = NULL,
               FormatCategory::FormatCategoryItems* matching_type = NULL);
    
    class NamedSummaryFormats
    {
    public:
        static bool
        Get (const ConstString &type, lldb::SummaryFormatSP &entry);
        
        static void
        Add (const ConstString &type, const lldb::SummaryFormatSP &entry);
        
        static bool
        Delete (const ConstString &type);
        
        static void
        Clear ();
        
        static void
        LoopThrough (SummaryFormat::SummaryCallback callback, void* callback_baton);
        
        static uint32_t
        GetCount ();
    };
    
    class Categories
    {
    public:
        
        static bool
        Get (const ConstString &category, lldb::FormatCategorySP &entry);
        
        static void
        Add (const ConstString &category);
        
        static bool
        Delete (const ConstString &category);
        
        static void
        Clear ();
        
        static void
        Clear (ConstString &category);
        
        static void
        Enable (ConstString& category);
        
        static void
        Disable (ConstString& category);
        
        static void
        LoopThrough (FormatManager::CategoryCallback callback, void* callback_baton);
        
        static uint32_t
        GetCount ();
    };
};

    
} // namespace lldb_private

#endif	// lldb_FormatManager_h_
