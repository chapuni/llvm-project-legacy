# This tool creates a shared library from the LLVM libraries. Generating this
# library is enabled by setting LLVM_BUILD_LLVM_DYLIB=yes on the CMake
# commandline. By default the shared library only exports the LLVM C API.


# You can configure which libraries from LLVM you want to include in the shared
# library by setting LLVM_DYLIB_COMPONENTS to a semi-colon delimited list of
# LLVM components. All compoenent names handled by llvm-config are valid.

if(NOT DEFINED LLVM_DYLIB_COMPONENTS)
  set(LLVM_LINK_COMPONENTS
    ${LLVM_TARGETS_TO_BUILD}
    Analysis
    AsmPrinter
    BitWriter
    CodeGen
    Core
    ExecutionEngine
    IPA
    IPO
    IRReader
    InstCombine
    Instrumentation
    Interpreter
    Linker
    MC
    MCJIT
    ObjCARCOpts
    Object
    ScalarOpts
    SelectionDAG
    Support
    Target
    TransformUtils
    Vectorize
    native
    )
else()
  set(LLVM_LINK_COMPONENTS ${LLVM_DYLIB_COMPONENTS})
endif()

add_definitions( -DLLVM_VERSION_INFO=\"${PACKAGE_VERSION}\" )

set(SOURCES
  libllvm.cpp
  )

add_llvm_library(LLVM SHARED ${SOURCES})

if (APPLE)
  set_property(TARGET LLVM APPEND_STRING PROPERTY
              LINK_FLAGS
              " -compatibility_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR} -current_version ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")
endif()

