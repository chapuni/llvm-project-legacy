//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <locale>

// template <class charT> class ctype_byname;

// const charT* tolower(charT* low, const charT* high) const;

#include <locale>
#include <string>
#include <cassert>

int main()
{
    {
        std::locale l("en_US.UTF-8");
        {
            typedef std::ctype<char> F;
            const F& f = std::use_facet<F>(l);
            std::string in("\xDA A\x07.a1");

            assert(f.tolower(&in[0], in.data() + in.size()) == in.data() + in.size());
            assert(in[0] == '\xDA');
            assert(in[1] == ' ');
            assert(in[2] == 'a');
            assert(in[3] == '\x07');
            assert(in[4] == '.');
            assert(in[5] == 'a');
            assert(in[6] == '1');
        }
    }
    {
        std::locale l("C");
        {
            typedef std::ctype<char> F;
            const F& f = std::use_facet<F>(l);
            std::string in("\xDA A\x07.a1");

            assert(f.tolower(&in[0], in.data() + in.size()) == in.data() + in.size());
            assert(in[0] == '\xDA');
            assert(in[1] == ' ');
            assert(in[2] == 'a');
            assert(in[3] == '\x07');
            assert(in[4] == '.');
            assert(in[5] == 'a');
            assert(in[6] == '1');
        }
    }
    {
        std::locale l("en_US.UTF-8");
        {
            typedef std::ctype<wchar_t> F;
            const F& f = std::use_facet<F>(l);
            std::wstring in(L"\xDA A\x07.a1");

            assert(f.tolower(&in[0], in.data() + in.size()) == in.data() + in.size());
            assert(in[0] == L'\xFA');
            assert(in[1] == L' ');
            assert(in[2] == L'a');
            assert(in[3] == L'\x07');
            assert(in[4] == L'.');
            assert(in[5] == L'a');
            assert(in[6] == L'1');
        }
    }
    {
        std::locale l("C");
        {
            typedef std::ctype<wchar_t> F;
            const F& f = std::use_facet<F>(l);
            std::wstring in(L"\xDA A\x07.a1");

            assert(f.tolower(&in[0], in.data() + in.size()) == in.data() + in.size());
            assert(in[0] == L'\xDA');
            assert(in[1] == L' ');
            assert(in[2] == L'a');
            assert(in[3] == L'\x07');
            assert(in[4] == L'.');
            assert(in[5] == L'a');
            assert(in[6] == L'1');
        }
    }
}
