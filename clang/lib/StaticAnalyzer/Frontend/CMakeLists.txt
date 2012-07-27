set(LLVM_NO_RTTI 1)

include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../Checkers )

add_clang_library(clangStaticAnalyzerFrontend
  AnalysisConsumer.cpp
  CheckerRegistration.cpp
  FrontendActions.cpp
  )

add_dependencies(clangStaticAnalyzerFrontend
  ClangAttrClasses
  ClangAttrList
  ClangCommentNodes
  ClangDeclNodes
  ClangDiagnosticCommon
  ClangDiagnosticFrontend
  ClangStmtNodes
  )
