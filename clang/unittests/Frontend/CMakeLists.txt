set(LLVM_LINK_COMPONENTS
  clangFrontend
  )

add_clang_unittest(FrontendTests
  FrontendActionTest.cpp
  )
