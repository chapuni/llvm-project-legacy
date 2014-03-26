// Test that large memset/memcpy/memmove check the entire range.

// RUN: %clangxx_asan -O0 -DTEST_MEMSET %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMSET
// RUN: %clangxx_asan -O1 -DTEST_MEMSET %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMSET
// RUN: %clangxx_asan -O2 -DTEST_MEMSET %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMSET
// RUN: %clangxx_asan -O3 -DTEST_MEMSET %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMSET

// RUN: %clangxx_asan -O0 -DTEST_MEMCPY %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMCPY
// RUN: %clangxx_asan -O1 -DTEST_MEMCPY %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMCPY
// RUN: %clangxx_asan -O2 -DTEST_MEMCPY %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMCPY
// RUN: %clangxx_asan -O3 -DTEST_MEMCPY %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMCPY

// RUN: %clangxx_asan -O0 -DTEST_MEMMOVE %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMMOVE
// RUN: %clangxx_asan -O1 -DTEST_MEMMOVE %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMMOVE
// RUN: %clangxx_asan -O2 -DTEST_MEMMOVE %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMMOVE
// RUN: %clangxx_asan -O3 -DTEST_MEMMOVE %s -o %t && not %t 2>&1 | \
// RUN:     FileCheck %s --check-prefix=CHECK-MEMMOVE

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sanitizer/asan_interface.h>

int main(int argc, char **argv) {
  char * volatile p = (char *)malloc(3000);
  __asan_poison_memory_region(p + 512, 16);
#if defined(TEST_MEMSET)
  memset(p, 0, 3000);
  assert(p[1] == 0);
  // CHECK-MEMSET: AddressSanitizer: use-after-poison on address
  // CHECK-MEMSET: in {{.*}}memset
#else
  char * volatile q = (char *)malloc(3000);
#if defined(TEST_MEMCPY)
  memcpy(q, p, 3000);
  // CHECK-MEMCPY: AddressSanitizer: use-after-poison on address
  // CHECK-MEMCPY: in {{.*}}memcpy
#elif defined(TEST_MEMMOVE)
  memmove(q, p, 3000);
  // CHECK-MEMMOVE: AddressSanitizer: use-after-poison on address
  // CHECK-MEMMOVE: in {{.*}}memmove
#endif
  assert(q[1] == 0);
  free(q);
#endif
  free(p);
  return 0;
}
