// RUN: %clang_cc1 -verify -fsyntax-only -Wshadow %s

int i;          // expected-note {{previous declaration is here}}

void foo() {
  int pass1;
  int i;        // expected-warning {{declaration shadows a variable in the global scope}} \
                // expected-note {{previous declaration is here}}
  {
    int pass2;
    int i;      // expected-warning {{declaration shadows a local variable}} \
                // expected-note {{previous declaration is here}}
    {
      int pass3;
      int i;    // expected-warning {{declaration shadows a local variable}}
    }
  }

  int __sync_fetch_and_add; // expected-warning {{declaration shadows a global built-in function}}
}
