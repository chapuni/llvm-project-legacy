//===-- OptionGroupArchitecture.cpp -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OptionGroupArchitecture.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes

using namespace lldb;
using namespace lldb_private;

OptionGroupArchitecture::OptionGroupArchitecture() :
    m_arch_str ()
{
}

OptionGroupArchitecture::~OptionGroupArchitecture ()
{
}

OptionDefinition g_file_option_table[] =
{
    { LLDB_OPT_SET_1 , false, "arch"    , 'a', required_argument, NULL, 0, eArgTypeArchitecture , "Specify the architecture for the target."},
};
const uint32_t k_num_file_options = sizeof(g_file_option_table)/sizeof(OptionDefinition);

uint32_t
OptionGroupArchitecture::GetNumDefinitions ()
{
    return k_num_file_options;
}

const OptionDefinition *
OptionGroupArchitecture::GetDefinitions ()
{
    return g_file_option_table;
}

bool
OptionGroupArchitecture::GetArchitecture (Platform *platform, ArchSpec &arch)
{
    if (m_arch_str.empty())
        arch.Clear();
    else
        arch.SetTriple(m_arch_str.c_str(), platform);
    return arch.IsValid();
}


Error
OptionGroupArchitecture::SetOptionValue (CommandInterpreter &interpreter,
                                 uint32_t option_idx,
                                 const char *option_arg)
{
    Error error;
    char short_option = (char) g_file_option_table[option_idx].short_option;

    switch (short_option)
    {
        case 'a':
            m_arch_str.assign (option_arg);
            break;

        default:
            error.SetErrorStringWithFormat ("Unrecognized option '%c'.\n", short_option);
            break;
    }

    return error;
}

void
OptionGroupArchitecture::OptionParsingStarting (CommandInterpreter &interpreter)
{
    m_arch_str.clear();
}

