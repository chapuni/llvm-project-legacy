set(LLVM_LINK_COMPONENTS
  clangLex
  )

add_clang_unittest(BasicTests
  FileManagerTest.cpp
  SourceManagerTest.cpp
  )
