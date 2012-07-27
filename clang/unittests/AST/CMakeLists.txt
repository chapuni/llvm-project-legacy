set(LLVM_LINK_COMPONENTS
  clangAST
  )

add_clang_unittest(ASTTests
  CommentLexer.cpp
  CommentParser.cpp
  )
