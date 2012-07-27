set(LLVM_LINK_COMPONENTS
  clangTooling
  )

add_clang_unittest(ToolingTests
  CommentHandlerTest.cpp
  CompilationDatabaseTest.cpp
  ToolingTest.cpp
  RecursiveASTVisitorTest.cpp
  RefactoringTest.cpp
  RewriterTest.cpp
  RefactoringCallbacksTest.cpp
  )
