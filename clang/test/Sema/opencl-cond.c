// RUN: %clang_cc1 %s -x cl -verify -pedantic -fsyntax-only

typedef __attribute__((ext_vector_type(4))) float float4;

float4 foo(float4 a, float4 b, float4 c, float4 d) { return a < b ? c : d; }
