//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// has_trivial_copy_assign

#include <type_traits>

template <class T>
void test_has_trivial_assign()
{
    static_assert( std::has_trivial_copy_assign<T>::value, "");
    static_assert(!std::has_trivial_copy_assign<const T>::value, "");
    static_assert( std::has_trivial_copy_assign<volatile T>::value, "");
    static_assert(!std::has_trivial_copy_assign<const volatile T>::value, "");
}

template <class T>
void test_has_not_trivial_assign()
{
    static_assert(!std::has_trivial_copy_assign<T>::value, "");
    static_assert(!std::has_trivial_copy_assign<const T>::value, "");
    static_assert(!std::has_trivial_copy_assign<volatile T>::value, "");
    static_assert(!std::has_trivial_copy_assign<const volatile T>::value, "");
}

class Empty
{
};

class NotEmpty
{
    virtual ~NotEmpty();
};

union Union {};

struct bit_zero
{
    int :  0;
};

class Abstract
{
    virtual ~Abstract() = 0;
};

struct A
{
    A& operator=(const A&);
};

int main()
{
    test_has_not_trivial_assign<void>();
    test_has_not_trivial_assign<A>();
    test_has_not_trivial_assign<int&>();
    test_has_not_trivial_assign<NotEmpty>();
    test_has_not_trivial_assign<Abstract>();

    test_has_trivial_assign<Union>();
    test_has_trivial_assign<Empty>();
    test_has_trivial_assign<int>();
    test_has_trivial_assign<double>();
    test_has_trivial_assign<int*>();
    test_has_trivial_assign<const int*>();
    test_has_trivial_assign<char[3]>();
    test_has_trivial_assign<char[3]>();
    test_has_trivial_assign<bit_zero>();
}
