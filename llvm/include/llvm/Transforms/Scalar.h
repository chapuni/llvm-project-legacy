//===-- Scalar.h - Scalar Transformations ------------------------*- C++ -*-==//
//
// This header file defines prototypes for accessor functions that expose passes
// in the Scalar transformations library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_H
#define LLVM_TRANSFORMS_SCALAR_H

class Pass;
class TargetData;
class BasicBlock;
class GetElementPtrInst;

//===----------------------------------------------------------------------===//
//
// Constant Propogation Pass - A worklist driven constant propogation pass
//
Pass *createConstantPropogationPass();


//===----------------------------------------------------------------------===//
//
// Sparse Conditional Constant Propogation Pass
//
Pass *createSCCPPass();


//===----------------------------------------------------------------------===//
//
// DeadInstElimination - This pass quickly removes trivially dead instructions
// without modifying the CFG of the function.  It is a BasicBlockPass, so it
// runs efficiently when queued next to other BasicBlockPass's.
//
Pass *createDeadInstEliminationPass();


//===----------------------------------------------------------------------===//
//
// DeadCodeElimination - This pass is more powerful than DeadInstElimination,
// because it is worklist driven that can potentially revisit instructions when
// their other instructions become dead, to eliminate chains of dead
// computations.
//
Pass *createDeadCodeEliminationPass();


//===----------------------------------------------------------------------===//
//
// AggressiveDCE - This pass uses the SSA based Aggressive DCE algorithm.  This
// algorithm assumes instructions are dead until proven otherwise, which makes
// it more successful are removing non-obviously dead instructions.
//
Pass *createAggressiveDCEPass();


//===----------------------------------------------------------------------===//
// 
// DecomposeMultiDimRefs - Convert multi-dimensional references consisting of
// any combination of 2 or more array and structure indices into a sequence of
// instructions (using getelementpr and cast) so that each instruction has at
// most one index (except structure references, which need an extra leading
// index of [0]).

// This pass decomposes all multi-dimensional references in a function.
Pass *createDecomposeMultiDimRefsPass();

// This function decomposes a single instance of such a reference.
// Return value: true if the instruction was replaced; false otherwise.
// 
bool DecomposeArrayRef(GetElementPtrInst* GEP);

//===----------------------------------------------------------------------===//
//
// GCSE - This pass is designed to be a very quick global transformation that
// eliminates global common subexpressions from a function.  It does this by
// examining the SSA value graph of the function, instead of doing slow
// bit-vector computations.
//
Pass *createGCSEPass();


//===----------------------------------------------------------------------===//
//
// InductionVariableSimplify - Transform induction variables in a program to all
// use a single cannonical induction variable per loop.
//
Pass *createIndVarSimplifyPass();


//===----------------------------------------------------------------------===//
//
// InstructionCombining - Combine instructions to form fewer, simple
//   instructions.  This pass does not modify the CFG, and has a tendancy to
//   make instructions dead, so a subsequent DCE pass is useful.
//
// This pass combines things like:
//    %Y = add int 1, %X
//    %Z = add int 1, %Y
// into:
//    %Z = add int 2, %X
//
Pass *createInstructionCombiningPass();


//===----------------------------------------------------------------------===//
//
// LICM - This pass is a simple natural loop based loop invariant code motion
// pass.
//
Pass *createLICMPass();


//===----------------------------------------------------------------------===//
//
// PiNodeInsertion - This pass inserts single entry Phi nodes into basic blocks
// that are preceeded by a conditional branch, where the branch gives
// information about the operands of the condition.  For example, this C code:
//   if (x == 0) { ... = x + 4;
// becomes:
//   if (x == 0) {
//     x2 = phi(x);    // Node that can hold data flow information about X
//     ... = x2 + 4;
//
// Since the direction of the condition branch gives information about X itself
// (whether or not it is zero), some passes (like value numbering or ABCD) can
// use the inserted Phi/Pi nodes as a place to attach information, in this case
// saying that X has a value of 0 in this scope.  The power of this analysis
// information is that "in the scope" translates to "for all uses of x2".
//
// This special form of Phi node is refered to as a Pi node, following the
// terminology defined in the "Array Bounds Checks on Demand" paper.
//
Pass *createPiNodeInsertionPass();


//===----------------------------------------------------------------------===//
//
// This pass is used to promote memory references to be register references.  A
// simple example of the transformation performed by this pass is:
//
//        FROM CODE                           TO CODE
//   %X = alloca int, uint 1                 ret int 42
//   store int 42, int *%X
//   %Y = load int* %X
//   ret int %Y
//
Pass *createPromoteMemoryToRegister();


//===----------------------------------------------------------------------===//
//
// This pass reassociates commutative expressions in an order that is designed
// to promote better constant propogation, GCSE, LICM, PRE...
//
// For example:  4 + (x + 5)  ->  x + (4 + 5)
//
Pass *createReassociatePass();

//===----------------------------------------------------------------------===//
//
// This pass eliminates correlated conditions, such as these:
//  if (X == 0)
//    if (X > 2)     // Known false
//    else
//      Y = X * Z;   // = 0
//
Pass *createCorrelatedExpressionEliminationPass();

//===----------------------------------------------------------------------===//
//
// CFG Simplification - Merge basic blocks, eliminate unreachable blocks,
// simplify terminator instructions, etc...
//
Pass *createCFGSimplificationPass();


//===----------------------------------------------------------------------===//
// These two passes convert malloc and free instructions to and from %malloc &
// %free function calls.  The LowerAllocations transformation is a target
// dependant tranformation because it depends on the size of data types and
// alignment constraints.
//
Pass *createLowerAllocationsPass(const TargetData &TD);
Pass *createRaiseAllocationsPass();


//===----------------------------------------------------------------------===//
//
// These functions removes symbols from functions and modules.
//
Pass *createSymbolStrippingPass();
Pass *createFullSymbolStrippingPass();

#endif
