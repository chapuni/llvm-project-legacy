set( LLVM_LINK_COMPONENTS
  clangTooling
  )

add_clang_executable(clang-check
  ClangCheck.cpp
  )
