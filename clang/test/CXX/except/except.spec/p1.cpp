// RUN: %clang_cc1 -std=c++0x -fexceptions -fcxx-exceptions -fsyntax-only -verify %s

// Simple parser tests, dynamic specification.

namespace dyn {

  struct X { };

  struct Y { };

  void f() throw() { }

  void g(int) throw(X) { }

  void h() throw(X, Y) { }

  class Class {
    void foo() throw (X, Y) { }
  };

  void (*fptr)() throw();

}

// Simple parser tests, noexcept specification.

namespace noex {

  void f1() noexcept { }
  void f2() noexcept (true) { }
  void f3() noexcept (false) { }
  void f4() noexcept (1 < 2) { }

  class CA1 {
    void foo() noexcept { }
    void bar() noexcept (true) { }
  };

  void (*fptr1)() noexcept;
  void (*fptr2)() noexcept (true);

}

namespace mix {

  void f() throw(int) noexcept { } // expected-error {{cannot have both}}
  void g() noexcept throw(int) { } // expected-error {{cannot have both}}

}

// Sema tests, noexcept specification

namespace noex {

  struct A {};

  void g1() noexcept(A()); // expected-error {{not contextually convertible}}
  void g2(bool b) noexcept(b); // expected-error {{argument to noexcept specifier must be a constant expression}}

}

namespace noexcept_unevaluated {
  template<typename T> bool f(T) {
    T* x = 1;
  }

  template<typename T>
  void g(T x) noexcept((sizeof(T) == sizeof(int)) || f(x)) { }

  void h() {
    g(1);
  }
}

namespace PR11084 {
  template<int X> struct A { 
    static int f() noexcept(1/X) { return 10; }  // expected-error{{argument to noexcept specifier must be a constant expression}}
  };

  void g() { A<0>::f(); } // expected-note{{in instantiation of template class 'PR11084::A<0>' requested here}}
}
