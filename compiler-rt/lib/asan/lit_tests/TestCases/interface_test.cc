// Check that user may include ASan interface header.
// RUN: %clang_asan -I %p/../../../include %s -o %t && %t
// RUN: %clang -I %p/../../../include %s -o %t && %t
#include <sanitizer/asan_interface.h>

int main() {
  return 0;
}
