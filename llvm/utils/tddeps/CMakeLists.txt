# For in-tree build. :(
set(CWD ${CMAKE_BINARY_DIR}/tddeps.dir)

file(MAKE_DIRECTORY ${CWD})

macro(append_dir var dirvar)
  list(APPEND "${var}" "${dirvar}=${${dirvar}}")
endmacro()

# Tell tddeps.py the directory layout.
append_dir(TDDEPS_DIRS LLVM_MAIN_SRC_DIR)
if(NOT "${LLVM_EXTERNAL_CLANG_BUILD}" STREQUAL "OFF"
    AND EXISTS "${LLVM_EXTERNAL_CLANG_SOURCE_DIR}")
  append_dir(TDDEPS_DIRS LLVM_EXTERNAL_CLANG_SOURCE_DIR)
  if(NOT "${LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_BUILD}" STREQUAL "OFF"
    AND EXISTS "${LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR}")
    append_dir(TDDEPS_DIRS LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR)
  endif()
endif()

cmake_minimum_required(VERSION 2.8.12.100)

set(TDDEPS_FRAG ${CWD}/tddeps.cmake)

# Duplicate tddeps.py into CMAKE_CURRENT_BINARY_DIR.
#   - It should be located before ninja's cycle 0.
#     tddeps.py might not exist if older tree were checked out.
set(TDDEPS_PY ${CWD}/tddeps.py)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tddeps.py ${TDDEPS_PY} COPYONLY)

# Rerun CMake if dependencies were updated.
set(action --action ${CMAKE_COMMAND} -H${CMAKE_SOURCE_DIR} -B${CMAKE_BINARY_DIR})

if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
  # Ninja can know build.ninja depends on tddeps.cmake in the cycle 0.
  set(action)

  # Ninja can work with stampfiles.
  set(STAMPDIR ${CWD}/stamps.dir)
  file(MAKE_DIRECTORY ${STAMPDIR})
  set(STAMP_OPT -s ${STAMPDIR})
endif()

execute_process(
  COMMAND ${PYTHON_EXECUTABLE} ${TDDEPS_PY}
  ${TDDEPS_DIRS}
  -o ${TDDEPS_FRAG}
  ${STAMP_OPT}
  ERROR_VARIABLE LLVMBUILDERRORS
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LLVMBUILDRESULT)

# On Win32, CMake doesn't properly handle piping the default output/error
# streams into the GUI console. So, we explicitly catch and report them.
if( NOT "${LLVMBUILDOUTPUT}" STREQUAL "")
  message(STATUS "llvm-build output: ${LLVMBUILDOUTPUT}")
endif()
if( NOT "${LLVMBUILDRESULT}" STREQUAL "0" )
  message(FATAL_ERROR
    "Unexpected failure executing llvm-build: ${LLVMBUILDERRORS}")
endif()

# They are imported with PARENT_SCOPE:
#   TDDEPS_*
#   TDDEPSET_*
# They are imported in the current scope:
#   STAMPS_DEPENDS (may not exist)
include(${TDDEPS_FRAG})

set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM ON)

add_custom_command(OUTPUT ${TDDEPS_FRAG}
  COMMAND ${PYTHON_EXECUTABLE} ${TDDEPS_PY} ${TDDEPS_DIRS}
  -i ${TDDEPS_FRAG}
  ${STAMP_OPT}
  ${action}
  DEPENDS ${TDDEPS_PY} ${STAMPS_DEPENDS}
  )

add_custom_target(tddeps DEPENDS ${TDDEPS_FRAG})
