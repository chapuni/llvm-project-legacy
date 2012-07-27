add_clang_library(clangFrontendTool
  ExecuteCompilerInvocation.cpp
  )

add_dependencies(clangFrontendTool
  ClangDiagnosticCommon
  ClangDiagnosticFrontend
  ClangDriverOptions
  )
