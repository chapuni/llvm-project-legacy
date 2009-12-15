namespace N4 {
  namespace N3 { }
}

class N3;

namespace N2 {
  namespace I1 { }
  namespace I4 = I1;
  namespace I5 { }
  namespace I1 { }
  
  void foo() {
    int N3;
    
    using
    // RUN: %clang_cc1 -fsyntax-only -code-completion-at=%s:16:10 %s -o - | FileCheck -check-prefix=CC1 %s
    // CHECK-CC1: I1 : 2
    // CHECK-CC1: I4 : 2
    // CHECK-CC1: I5 : 2
    // CHECK-CC1: N2 : 4
    // CHECK-CC1: N3 : 4
    // CHECK-CC1-NEXT: N4 : 4

