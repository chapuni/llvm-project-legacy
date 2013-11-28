add_clang_library(clangRewriteCore
  DeltaTree.cpp
  HTMLRewrite.cpp
  RewriteRope.cpp
  Rewriter.cpp
  TokenRewriter.cpp
  )

target_link_libraries(clangRewriteCore
  clangBasic
  clangAST
  clangParse
  )
