if (WIN32 AND NOT CYGWIN)
  # llvm-lit needs suffix.py for multiprocess to find a main module.
  set(suffix .py)
endif ()

if (NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
  foreach (BUILD_MODE ${CMAKE_CONFIGURATION_TYPES})
    configure_file(
      llvm-lit.in
      ${CMAKE_BINARY_DIR}/${BUILD_MODE}/bin/llvm-lit${suffix}
      )
  endforeach ()
else()
  set(BUILD_MODE .)
  configure_file(
    llvm-lit.in
    ${CMAKE_BINARY_DIR}/bin/llvm-lit${suffix}
    )
endif ()
