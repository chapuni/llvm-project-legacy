set( LLVM_LINK_COMPONENTS
  clangARCMigrate
  )

add_clang_executable(arcmt-test
  arcmt-test.cpp
  )
