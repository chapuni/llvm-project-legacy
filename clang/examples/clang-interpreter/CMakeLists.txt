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

target_link_libraries(clang-interpreter
  clangFrontend
  clangSerialization
  clangDriver
  clangCodeGen
  clangSema
  clangStaticAnalyzerFrontend
  clangStaticAnalyzerCheckers
  clangStaticAnalyzerCore
  clangAnalysis
  clangRewrite
  clangAST
  clangParse
  clangLex
  clangBasic
  )
