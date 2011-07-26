// RUN: %clang_cc1 -emit-llvm %s -o - | FileCheck %s
struct A { short s; short t; int i; };
// CHECK: %a = alloca %struct.A, align 4
// CHECK: call void @llvm.memset.p0i8.i64(i8* %tmp, i8 0, i64 8, i32 4, i1 false)
void q() { struct A a = {0}; }
