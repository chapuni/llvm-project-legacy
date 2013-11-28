if(NOT CLANG_BUILD_EXAMPLES)
  set_property(DIRECTORY PROPERTY EXCLUDE_FROM_ALL ON)
endif()

list(APPEND LLVM_COMMON_DEPENDS ${CLANG_TBLGEN_DEPS})

add_subdirectory(analyzer-plugin)
add_subdirectory(clang-interpreter)
add_subdirectory(PrintFunctionNames)
