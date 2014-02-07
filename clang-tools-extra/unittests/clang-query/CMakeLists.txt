set(LLVM_LINK_COMPONENTS
  support
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../clang-query
  )

add_extra_unittest(ClangQueryTests #TDDEPS:exclude
  QueryEngineTest.cpp
  QueryParserTest.cpp
  )

target_link_libraries(ClangQueryTests
  clangAST
  clangASTMatchers
  clangQuery
  clangTooling
  )
