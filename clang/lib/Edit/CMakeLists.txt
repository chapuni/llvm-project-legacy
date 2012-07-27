add_clang_library(clangEdit
  Commit.cpp
  EditedSource.cpp
  RewriteObjCFoundationAPI.cpp
  )

add_dependencies(clangEdit
  ClangAttrClasses
  ClangAttrList
  ClangCommentNodes
  ClangDeclNodes
  ClangDiagnosticCommon
  ClangStmtNodes
  )
