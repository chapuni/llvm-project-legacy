// Test this without pch.
// RUN: %clang_cc1 -include %S/pragma-weak.h %s -verify -emit-llvm -o - | FileCheck %s

// Test with pch.
// RUN: %clang_cc1 -x c-header -emit-pch -o %t %S/pragma-weak.h
// RUN: %clang_cc1 -include-pch %t %s -verify -emit-llvm -o - | FileCheck %s

// CHECK: @weakvar = weak global i32 0
int weakvar;
// expected-warning {{weak identifier 'undeclaredvar' never declared}}
