// PR947
// RUN: %clang_cc1 %s -emit-llvm -o - 

void foo() {
    void *ptr;
  label:
    ptr = &&label;

    goto *ptr;
  }
