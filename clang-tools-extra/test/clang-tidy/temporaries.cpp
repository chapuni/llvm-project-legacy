// RUN: clang-tidy -checks=clang-analyzer-core.NullDereference -disable-checks='' -analyze-temporary-dtors %s -- | FileCheck %s

struct NoReturnDtor {
  ~NoReturnDtor() __attribute__((noreturn));
};

extern bool check(const NoReturnDtor &);

// CHECK-NOT: warning
void testNullPointerDereferencePositive() {
  int *value = 0;
  // CHECK: [[@LINE+1]]:10: warning: Dereference of null pointer (loaded from variable 'value') [clang-analyzer-core.NullDereference]
  *value = 1;
}

// CHECK-NOT: warning
void testNullPointerDereference() {
  int *value = 0;
  if (check(NoReturnDtor())) {
    // This unreachable code causes a warning if we don't run with -analyze-temporary-dtors
    *value = 1;
  }
}
