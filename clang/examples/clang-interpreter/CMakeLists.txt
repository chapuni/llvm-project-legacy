set(LLVM_LINK_COMPONENTS
  clangCodeGen
  jit
  interpreter
  nativecodegen
  )

add_clang_executable(clang-interpreter
  main.cpp
  )

add_dependencies(clang-interpreter
  clang-headers
  )
