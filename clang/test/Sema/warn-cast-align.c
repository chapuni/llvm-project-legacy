// RUN: %clang_cc1 -triple x86_64-apple-darwin10 -fsyntax-only -Wcast-align -verify %s

// Simple casts.
void test0(char *P) {
  char *a  = (char*)  P;
  short *b = (short*) P; // expected-warning {{cast from 'char *' to 'short *' increases required alignment from 1 to 2}}
  int *c   = (int*)   P; // expected-warning {{cast from 'char *' to 'int *' increases required alignment from 1 to 4}}
}

// Casts from void* are a special case.
void test1(void *P) {
  char *a  = (char*)  P;
  short *b = (short*) P;
  int *c   = (int*)   P;

  const volatile void *P2 = P;
  char *d  = (char*)  P2;
  short *e = (short*) P2;
  int *f   = (int*)   P2;

  const char *g  = (const char*)  P2;
  const short *h = (const short*) P2;
  const int *i   = (const int*)   P2;

  const volatile char *j  = (const volatile char*)  P2;
  const volatile short *k = (const volatile short*) P2;
  const volatile int *l   = (const volatile int*)   P2;
}

// Aligned struct.
__attribute__((aligned(16))) struct A {
  char buffer[16];
};
void test2(char *P) {
  struct A *a = (struct A*) P; // expected-warning {{cast from 'char *' to 'struct A *' increases required alignment from 1 to 16}}
}

// Incomplete type.
void test3(char *P) {
  struct B *b = (struct B*) P;
}
