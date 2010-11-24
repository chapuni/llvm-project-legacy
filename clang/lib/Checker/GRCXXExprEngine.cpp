//===- GRCXXExprEngine.cpp - C++ expr evaluation engine ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the C++ expression evaluation engine.
//
//===----------------------------------------------------------------------===//

#include "clang/Checker/PathSensitive/AnalysisManager.h"
#include "clang/Checker/PathSensitive/GRExprEngine.h"
#include "clang/AST/DeclCXX.h"

using namespace clang;

namespace {
class CallExprWLItem {
public:
  CallExpr::const_arg_iterator I;
  ExplodedNode *N;

  CallExprWLItem(const CallExpr::const_arg_iterator &i, ExplodedNode *n)
    : I(i), N(n) {}
};
}

void GRExprEngine::EvalArguments(ConstExprIterator AI, ConstExprIterator AE,
                                 const FunctionProtoType *FnType, 
                                 ExplodedNode *Pred, ExplodedNodeSet &Dst,
                                 bool FstArgAsLValue) {


  llvm::SmallVector<CallExprWLItem, 20> WorkList;
  WorkList.reserve(AE - AI);
  WorkList.push_back(CallExprWLItem(AI, Pred));

  while (!WorkList.empty()) {
    CallExprWLItem Item = WorkList.back();
    WorkList.pop_back();

    if (Item.I == AE) {
      Dst.insert(Item.N);
      continue;
    }

    // Evaluate the argument.
    ExplodedNodeSet Tmp;
    bool VisitAsLvalue = FstArgAsLValue;
    if (FstArgAsLValue) {
      FstArgAsLValue = false;
    } else {
      const unsigned ParamIdx = Item.I - AI;
      VisitAsLvalue = FnType && ParamIdx < FnType->getNumArgs() 
        ? FnType->getArgType(ParamIdx)->isReferenceType()
        : false;
    }

    if (VisitAsLvalue)
      VisitLValue(*Item.I, Item.N, Tmp);
    else
      Visit(*Item.I, Item.N, Tmp);

    ++(Item.I);
    for (ExplodedNodeSet::iterator NI=Tmp.begin(), NE=Tmp.end(); NI != NE; ++NI)
      WorkList.push_back(CallExprWLItem(Item.I, *NI));
  }
}

const CXXThisRegion *GRExprEngine::getCXXThisRegion(const CXXRecordDecl *D,
                                                 const StackFrameContext *SFC) {
  Type *T = D->getTypeForDecl();
  QualType PT = getContext().getPointerType(QualType(T, 0));
  return ValMgr.getRegionManager().getCXXThisRegion(PT, SFC);
}

void GRExprEngine::CreateCXXTemporaryObject(const Expr *Ex, ExplodedNode *Pred,
                                            ExplodedNodeSet &Dst) {
  ExplodedNodeSet Tmp;
  Visit(Ex, Pred, Tmp);
  for (ExplodedNodeSet::iterator I = Tmp.begin(), E = Tmp.end(); I != E; ++I) {
    const GRState *state = GetState(*I);

    // Bind the temporary object to the value of the expression. Then bind
    // the expression to the location of the object.
    SVal V = state->getSVal(Ex);

    const MemRegion *R =
      ValMgr.getRegionManager().getCXXObjectRegion(Ex,
                                                   Pred->getLocationContext());

    state = state->bindLoc(loc::MemRegionVal(R), V);
    MakeNode(Dst, Ex, Pred, state->BindExpr(Ex, loc::MemRegionVal(R)));
  }
}

void GRExprEngine::VisitCXXConstructExpr(const CXXConstructExpr *E, 
                                         const MemRegion *Dest,
                                         ExplodedNode *Pred,
                                         ExplodedNodeSet &Dst, bool asLValue) {
  if (!Dest)
    Dest = ValMgr.getRegionManager().getCXXObjectRegion(E,
                                                    Pred->getLocationContext());

  if (E->isElidable()) {
    VisitAggExpr(E->getArg(0), Dest, Pred, Dst);
    return;
  }

  const CXXConstructorDecl *CD = E->getConstructor();
  assert(CD);

  if (!(CD->isThisDeclarationADefinition() && AMgr.shouldInlineCall()))
    // FIXME: invalidate the object.
    return;

  
  // Evaluate other arguments.
  ExplodedNodeSet ArgsEvaluated;
  const FunctionProtoType *FnType = CD->getType()->getAs<FunctionProtoType>();
  EvalArguments(E->arg_begin(), E->arg_end(), FnType, Pred, ArgsEvaluated);
  // The callee stack frame context used to create the 'this' parameter region.
  const StackFrameContext *SFC = AMgr.getStackFrame(CD, 
                                                    Pred->getLocationContext(),
                        E, asLValue, Builder->getBlock(), Builder->getIndex());

  const CXXThisRegion *ThisR =getCXXThisRegion(E->getConstructor()->getParent(),
                                               SFC);

  CallEnter Loc(E, SFC, Pred->getLocationContext());
  for (ExplodedNodeSet::iterator NI = ArgsEvaluated.begin(),
                                 NE = ArgsEvaluated.end(); NI != NE; ++NI) {
    const GRState *state = GetState(*NI);
    // Setup 'this' region, so that the ctor is evaluated on the object pointed
    // by 'Dest'.
    state = state->bindLoc(loc::MemRegionVal(ThisR), loc::MemRegionVal(Dest));
    ExplodedNode *N = Builder->generateNode(Loc, state, Pred);
    if (N)
      Dst.Add(N);
  }
}

void GRExprEngine::VisitCXXDestructor(const CXXDestructorDecl *DD,
                                      const MemRegion *Dest,
                                      const Stmt *S,
                                      ExplodedNode *Pred, 
                                      ExplodedNodeSet &Dst) {
  if (!(DD->isThisDeclarationADefinition() && AMgr.shouldInlineCall()))
    return;
  // Create the context for 'this' region.
  const StackFrameContext *SFC = AMgr.getStackFrame(DD,
                                                    Pred->getLocationContext(),
                                                 S, false, Builder->getBlock(),
                                                    Builder->getIndex());

  const CXXThisRegion *ThisR = getCXXThisRegion(DD->getParent(), SFC);

  CallEnter PP(S, SFC, Pred->getLocationContext());

  const GRState *state = Pred->getState();
  state = state->bindLoc(loc::MemRegionVal(ThisR), loc::MemRegionVal(Dest));
  ExplodedNode *N = Builder->generateNode(PP, state, Pred);
  if (N)
    Dst.Add(N);
}

void GRExprEngine::VisitCXXMemberCallExpr(const CXXMemberCallExpr *MCE, 
                                          ExplodedNode *Pred, 
                                          ExplodedNodeSet &Dst) {
  // Get the method type.
  const FunctionProtoType *FnType = 
                       MCE->getCallee()->getType()->getAs<FunctionProtoType>();
  assert(FnType && "Method type not available");

  // Evaluate explicit arguments with a worklist.
  ExplodedNodeSet ArgsEvaluated;
  EvalArguments(MCE->arg_begin(), MCE->arg_end(), FnType, Pred, ArgsEvaluated);
 
  // Evaluate the implicit object argument.
  ExplodedNodeSet AllArgsEvaluated;
  const MemberExpr *ME = dyn_cast<MemberExpr>(MCE->getCallee()->IgnoreParens());
  if (!ME)
    return;
  Expr *ObjArgExpr = ME->getBase();
  for (ExplodedNodeSet::iterator I = ArgsEvaluated.begin(), 
                                 E = ArgsEvaluated.end(); I != E; ++I) {
    if (ME->isArrow())
      Visit(ObjArgExpr, *I, AllArgsEvaluated);
    else
      VisitLValue(ObjArgExpr, *I, AllArgsEvaluated);
  }

  // Now evaluate the call itself.
  const CXXMethodDecl *MD = cast<CXXMethodDecl>(ME->getMemberDecl());
  assert(MD && "not a CXXMethodDecl?");
  EvalMethodCall(MCE, MD, ObjArgExpr, Pred, AllArgsEvaluated, Dst);
}

void GRExprEngine::VisitCXXOperatorCallExpr(const CXXOperatorCallExpr *C,
                                            ExplodedNode *Pred,
                                            ExplodedNodeSet &Dst) {
  const CXXMethodDecl *MD = dyn_cast_or_null<CXXMethodDecl>(C->getCalleeDecl());
  if (!MD) {
    // If the operator doesn't represent a method call treat as regural call.
    VisitCall(C, Pred, C->arg_begin(), C->arg_end(), Dst, false);
    return;
  }

  // Determine the type of function we're calling (if available).
  const FunctionProtoType *Proto = NULL;
  QualType FnType = C->getCallee()->IgnoreParens()->getType();
  if (const PointerType *FnTypePtr = FnType->getAs<PointerType>())
    Proto = FnTypePtr->getPointeeType()->getAs<FunctionProtoType>();

  // Evaluate arguments treating the first one (object method is called on)
  // as alvalue.
  ExplodedNodeSet ArgsEvaluated;
  EvalArguments(C->arg_begin(), C->arg_end(), Proto, Pred, ArgsEvaluated, true);

  // Now evaluate the call itself.
  EvalMethodCall(C, MD, C->getArg(0), Pred, ArgsEvaluated, Dst);
}

void GRExprEngine::EvalMethodCall(const CallExpr *MCE, const CXXMethodDecl *MD,
                                  const Expr *ThisExpr, ExplodedNode *Pred,
                                  ExplodedNodeSet &Src, ExplodedNodeSet &Dst) {
  // Allow checkers to pre-visit the member call.
  ExplodedNodeSet PreVisitChecks;
  CheckerVisit(MCE, PreVisitChecks, Src, PreVisitStmtCallback);

  if (!(MD->isThisDeclarationADefinition() && AMgr.shouldInlineCall())) {
    // FIXME: conservative method call evaluation.
    CheckerVisit(MCE, Dst, PreVisitChecks, PostVisitStmtCallback);
    return;
  }

  const StackFrameContext *SFC = AMgr.getStackFrame(MD, 
                                                    Pred->getLocationContext(),
                                                    MCE, false,
                                                    Builder->getBlock(), 
                                                    Builder->getIndex());
  const CXXThisRegion *ThisR = getCXXThisRegion(MD->getParent(), SFC);
  CallEnter Loc(MCE, SFC, Pred->getLocationContext());
  for (ExplodedNodeSet::iterator I = PreVisitChecks.begin(),
         E = PreVisitChecks.end(); I != E; ++I) {
    // Set up 'this' region.
    const GRState *state = GetState(*I);
    state = state->bindLoc(loc::MemRegionVal(ThisR), state->getSVal(ThisExpr));
    Dst.Add(Builder->generateNode(Loc, state, *I));
  }
}

void GRExprEngine::VisitCXXNewExpr(const CXXNewExpr *CNE, ExplodedNode *Pred,
                                   ExplodedNodeSet &Dst) {
  if (CNE->isArray()) {
    // FIXME: allocating an array has not been handled.
    return;
  }

  unsigned Count = Builder->getCurrentBlockCount();
  DefinedOrUnknownSVal SymVal = getValueManager().getConjuredSymbolVal(NULL,CNE,
                                                         CNE->getType(), Count);
  const MemRegion *NewReg = cast<loc::MemRegionVal>(SymVal).getRegion();

  QualType ObjTy = CNE->getType()->getAs<PointerType>()->getPointeeType();

  const ElementRegion *EleReg = 
                         getStoreManager().GetElementZeroRegion(NewReg, ObjTy);

  // Evaluate constructor arguments.
  const FunctionProtoType *FnType = NULL;
  const CXXConstructorDecl *CD = CNE->getConstructor();
  if (CD)
    FnType = CD->getType()->getAs<FunctionProtoType>();
  ExplodedNodeSet ArgsEvaluated;
  EvalArguments(CNE->constructor_arg_begin(), CNE->constructor_arg_end(),
                FnType, Pred, ArgsEvaluated);

  // Initialize the object region and bind the 'new' expression.
  for (ExplodedNodeSet::iterator I = ArgsEvaluated.begin(), 
                                 E = ArgsEvaluated.end(); I != E; ++I) {
    const GRState *state = GetState(*I);

    if (ObjTy->isRecordType()) {
      state = state->InvalidateRegion(EleReg, CNE, Count);
    } else {
      if (CNE->hasInitializer()) {
        SVal V = state->getSVal(*CNE->constructor_arg_begin());
        state = state->bindLoc(loc::MemRegionVal(EleReg), V);
      } else {
        // Explicitly set to undefined, because currently we retrieve symbolic
        // value from symbolic region.
        state = state->bindLoc(loc::MemRegionVal(EleReg), UndefinedVal());
      }
    }
    state = state->BindExpr(CNE, loc::MemRegionVal(EleReg));
    MakeNode(Dst, CNE, *I, state);
  }
}

void GRExprEngine::VisitCXXDeleteExpr(const CXXDeleteExpr *CDE, 
                                      ExplodedNode *Pred,ExplodedNodeSet &Dst) {
  // Should do more checking.
  ExplodedNodeSet ArgEvaluated;
  Visit(CDE->getArgument(), Pred, ArgEvaluated);
  for (ExplodedNodeSet::iterator I = ArgEvaluated.begin(), 
                                 E = ArgEvaluated.end(); I != E; ++I) {
    const GRState *state = GetState(*I);
    MakeNode(Dst, CDE, *I, state);
  }
}

void GRExprEngine::VisitCXXThisExpr(const CXXThisExpr *TE, ExplodedNode *Pred,
                                    ExplodedNodeSet &Dst) {
  // Get the this object region from StoreManager.
  const MemRegion *R =
    ValMgr.getRegionManager().getCXXThisRegion(
                                  getContext().getCanonicalType(TE->getType()),
                                               Pred->getLocationContext());

  const GRState *state = GetState(Pred);
  SVal V = state->getSVal(loc::MemRegionVal(R));
  MakeNode(Dst, TE, Pred, state->BindExpr(TE, V));
}
