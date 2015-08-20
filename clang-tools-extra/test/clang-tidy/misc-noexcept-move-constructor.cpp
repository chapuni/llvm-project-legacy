// RUN: %python %S/check_clang_tidy.py %s misc-noexcept-move-constructor %t

class A {
  A(A &&);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: move constructors should be marked noexcept [misc-noexcept-move-constructor]
  A &operator=(A &&);
  // CHECK-MESSAGES: :[[@LINE-1]]:6: warning: move assignment operators should
};

struct B {
  static constexpr bool kFalse = false;
  B(B &&) noexcept(kFalse);
  // CHECK-MESSAGES: :[[@LINE-1]]:20: warning: noexcept specifier on the move constructor evaluates to 'false' [misc-noexcept-move-constructor]
};

class OK {};

void f() {
  OK a;
  a = OK();
}

class OK1 {
 public:
  OK1();
  OK1(const OK1 &);
  OK1(OK1&&) noexcept;
  OK1 &operator=(OK1 &&) noexcept;
  void f();
  void g() noexcept;
};

class OK2 {
  static constexpr bool kTrue = true;

public:
  OK2(OK2 &&) noexcept(true) {}
  OK2 &operator=(OK2 &&) noexcept(kTrue) { return *this; }
};

struct OK3 {
  OK3(OK3 &&) noexcept(false) {}
  OK3 &operator=(OK3 &&) = delete;
};
