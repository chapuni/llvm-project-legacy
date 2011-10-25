// RUN: %clang_cc1 -fms-extensions %s -verify

struct Nontemplate {
  typedef int type;
};

template<typename T>
struct X {
  __if_exists(Nontemplate::type) {
    typedef Nontemplate::type type;
  }

  __if_exists(Nontemplate::value) {
    typedef Nontemplate::value type2;
  }

  __if_not_exists(Nontemplate::value) {
    typedef int type3;
  }

  __if_exists(T::X) { // expected-warning{{dependent __if_exists declarations are ignored}}
    typedef T::X type4;
  }
};

X<int>::type i1;
X<int>::type2 i2; // expected-error{{no type named 'type2' in 'X<int>'}}
X<int>::type3 i3;
X<int>::type4 i4; // expected-error{{no type named 'type4' in 'X<int>'}}

struct HasFoo { 
  void foo();
};
struct HasBar { 
  void bar(int);
  void bar(float);
};

template<typename T>
void f(T t) {
  __if_exists(T::foo) {
    { }
    t.foo();
  }

  __if_not_exists(T::bar) {
    int *i = t; // expected-error{{no viable conversion from 'HasFoo' to 'int *'}}
    { }
  }
}

template void f(HasFoo); // expected-note{{in instantiation of function template specialization 'f<HasFoo>' requested here}}
template void f(HasBar);
