set(LLVM_LINK_COMPONENTS jit mcjit nativecodegen)
set(LLVM_REQUIRES_EH 1)

add_llvm_example(ExceptionDemo
  ExceptionDemo.cpp
  DEPENDS intrinsics_gen
  )

set_target_properties(ExceptionDemo PROPERTIES ENABLE_EXPORTS 1)
