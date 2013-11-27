add_llvm_library(LLVMVectorize
  BBVectorize.cpp
  Vectorize.cpp
  LoopVectorize.cpp
  SLPVectorizer.cpp
  DEPENDS intrinsics_gen
  )
