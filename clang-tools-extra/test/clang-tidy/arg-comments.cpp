// RUN: %python %S/check_clang_tidy.py %s misc-argument-comment %t

// FIXME: clang-tidy should provide a -verify mode to make writing these checks
// easier and more accurate.

void ffff(int xxxx, int yyyy);

void f(int x, int y);
void g() {
  // CHECK-MESSAGES: [[@LINE+4]]:5: warning: argument name 'y' in comment does not match parameter name 'x'
  // CHECK-MESSAGES: :[[@LINE-3]]:12: note: 'x' declared here
  // CHECK-MESSAGES: [[@LINE+2]]:14: warning: argument name 'z' in comment does not match parameter name 'y'
  // CHECK-MESSAGES: :[[@LINE-5]]:19: note: 'y' declared here
  f(/*y=*/0, /*z=*/0);
}

struct Closure {};

template <typename T1, typename T2>
Closure *NewCallback(void (*f)(T1, T2), T1 arg1, T2 arg2) { return nullptr; }

template <typename T1, typename T2>
Closure *NewPermanentCallback(void (*f)(T1, T2), T1 arg1, T2 arg2) { return nullptr; }

void h() {
  (void)NewCallback(&ffff, /*xxxx=*/11, /*yyyy=*/22);
  (void)NewPermanentCallback(&ffff, /*xxxx=*/11, /*yyyy=*/22);
}

template<typename... Args>
void variadic(Args&&... args);

template<typename... Args>
void variadic2(int zzz, Args&&... args);

void templates() {
  variadic(/*xxx=*/0, /*yyy=*/1);
  variadic2(/*zzZ=*/0, /*xxx=*/1, /*yyy=*/2);
  // CHECK-MESSAGES: [[@LINE-1]]:13: warning: argument name 'zzZ' in comment does not match parameter name 'zzz'
}
