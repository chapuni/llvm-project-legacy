// RUN: clang-cc -triple arm-unknown-unknown -target-abi apcs-gnu -fsyntax-only -verify %s

struct s0 { double f0; int f1; };
char chk0[__alignof__(struct s0) == 4 ? 1 : -1]; 
