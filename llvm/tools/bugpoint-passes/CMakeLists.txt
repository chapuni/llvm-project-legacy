if( NOT LLVM_BUILD_TOOLS )
  set(EXCLUDE_FROM_ALL ON)
endif()

add_llvm_loadable_module( BugpointPasses
  TestPasses.cpp
  DEPENDS
    intrinsics_gen

    # Ninja generator cannot add targets to link-only target.
    # add_dependencies(BugpointPasses bugpoint)
    # Add here instead.
    bugpoint
  )
