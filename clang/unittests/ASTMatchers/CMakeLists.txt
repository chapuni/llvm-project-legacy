set(LLVM_LINK_COMPONENTS
  clangTooling
  )

add_clang_unittest(ASTMatchersTests
  ASTMatchersTest.cpp)
