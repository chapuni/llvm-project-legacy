set(LLVM_LINK_COMPONENTS support)
set(LLVM_USED_LIBS clangBasic clangAST)

add_clang_library(clangASTMatchers
  ASTMatchFinder.cpp
  ASTMatchersInternal.cpp
  )

add_dependencies(clangASTMatchers
  ClangAttrClasses
  ClangAttrList
  ClangCommentNodes
  ClangDeclNodes
  ClangDiagnosticCommon
  ClangStmtNodes
  )
