// RUN: %python %S/check_clang_tidy.py %s misc-use-override %t -- -std=c++98

struct Base {
  virtual ~Base() {}
  virtual void a();
  virtual void b();
};

struct SimpleCases : public Base {
public:
  virtual ~SimpleCases();
  // CHECK-FIXES: {{^}}  virtual ~SimpleCases();

  void a();
  // CHECK-FIXES: {{^}}  void a();

  virtual void b();
  // CHECK-FIXES: {{^}}  virtual void b();
};
