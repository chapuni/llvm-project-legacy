//===- SSEDomainFix.cpp - Use proper int/float domain for SSE ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the SSEDomainFix pass.
//
// Some SSE instructions like mov, and, or, xor are available in different
// variants for different operand types. These variant instructions are
// equivalent, but on Nehalem and newer cpus there is extra latency
// transferring data between integer and floating point domains.
//
// This pass changes the variant instructions to minimize domain crossings.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sse-domain-fix"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

/// Allocate objects from a pool, allow objects to be recycled, and provide a
/// way of deleting everything.
template<typename T, unsigned PageSize = 64>
class PoolAllocator {
  std::vector<T*> Pages, Avail;
public:
  ~PoolAllocator() { Clear(); }

  T* Alloc() {
    if (Avail.empty()) {
      T *p = new T[PageSize];
      Pages.push_back(p);
      Avail.reserve(PageSize);
      for (unsigned n = 0; n != PageSize; ++n)
        Avail.push_back(p+n);
    }
    T *p = Avail.back();
    Avail.pop_back();
    return p;
  }

  // Allow object to be reallocated. It won't be reconstructed.
  void Recycle(T *p) {
    p->clear();
    Avail.push_back(p);
  }

  // Destroy all objects, make sure there are no external pointers to them.
  void Clear() {
    Avail.clear();
    while (!Pages.empty()) {
      delete[] Pages.back();
      Pages.pop_back();
    }
  }
};

/// A DomainValue is a bit like LiveIntervals' ValNo, but it laso keeps track
/// of execution domains.
///
/// An open DomainValue represents a set of instructions that can still switch
/// execution domain. Multiple registers may refer to the same open
/// DomainValue - they will eventually be collapsed to the same execution
/// domain.
///
/// A collapsed DomainValue represents a single register that has been forced
/// into one of more execution domains. There is a separate collapsed
/// DomainValue for each register, but it may contain multiple execution
/// domains. A register value is initially created in a single execution
/// domain, but if we were forced to pay the penalty of a domain crossing, we
/// keep track of the fact the the register is now available in multiple
/// domains.
struct DomainValue {
  // Basic reference counting.
  unsigned Refs;

  // Available domains. For an open DomainValue, it is the still possible
  // domains for collapsing. For a collapsed DomainValue it is the domains where
  // the register is available for free.
  unsigned Mask;

  // Position of the last defining instruction.
  unsigned Dist;

  // Twiddleable instructions using or defining these registers.
  SmallVector<MachineInstr*, 8> Instrs;

  // Collapsed DomainValue have no instructions to twiddle - it simply keeps
  // track of the domains where the registers are already available.
  bool collapsed() const { return Instrs.empty(); }

  // Is any domain in mask available?
  bool compat(unsigned mask) const {
    return Mask & mask;
  }

  // Mark domain as available.
  void add(unsigned domain) {
    Mask |= 1u << domain;
  }

  // First domain available in mask.
  unsigned firstDomain() const {
    return CountTrailingZeros_32(Mask);
  }

  DomainValue() { clear(); }

  void clear() {
    Refs = Mask = Dist = 0;
    Instrs.clear();
  }
};

static const unsigned NumRegs = 16;

class SSEDomainFixPass : public MachineFunctionPass {
  static char ID;
  PoolAllocator<DomainValue> Pool;

  MachineFunction *MF;
  const X86InstrInfo *TII;
  const TargetRegisterInfo *TRI;
  MachineBasicBlock *MBB;
  DomainValue **LiveRegs;
  typedef DenseMap<MachineBasicBlock*,DomainValue**> LiveOutMap;
  LiveOutMap LiveOuts;
  unsigned Distance;

public:
  SSEDomainFixPass() : MachineFunctionPass(&ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  virtual bool runOnMachineFunction(MachineFunction &MF);

  virtual const char *getPassName() const {
    return "SSE execution domain fixup";
  }

private:
  // Register mapping.
  int RegIndex(unsigned Reg);

  // LiveRegs manipulations.
  void SetLiveReg(int rx, DomainValue *DV);
  void Kill(int rx);
  void Force(int rx, unsigned domain);
  void Collapse(DomainValue *dv, unsigned domain);
  bool Merge(DomainValue *A, DomainValue *B);

  void enterBasicBlock();
  void visitGenericInstr(MachineInstr*);
  void visitSoftInstr(MachineInstr*, unsigned mask);
  void visitHardInstr(MachineInstr*, unsigned domain);

};
}

char SSEDomainFixPass::ID = 0;

/// Translate TRI register number to an index into our smaller tables of
/// interesting registers. Return -1 for boring registers.
int SSEDomainFixPass::RegIndex(unsigned reg) {
  // Registers are sorted lexicographically.
  // We just need them to be consecutive, ordering doesn't matter.
  assert(X86::XMM9 == X86::XMM0+NumRegs-1 && "Unexpected sort");
  reg -= X86::XMM0;
  return reg < NumRegs ? reg : -1;
}

/// Set LiveRegs[rx] = dv, updating reference counts.
void SSEDomainFixPass::SetLiveReg(int rx, DomainValue *dv) {
  assert(unsigned(rx) < NumRegs && "Invalid index");
  if (!LiveRegs)
    LiveRegs = (DomainValue**)calloc(sizeof(DomainValue*), NumRegs);

  if (LiveRegs[rx] == dv)
    return;
  if (LiveRegs[rx]) {
    assert(LiveRegs[rx]->Refs && "Bad refcount");
    if (--LiveRegs[rx]->Refs == 0) Pool.Recycle(LiveRegs[rx]);
  }
  LiveRegs[rx] = dv;
  if (dv) ++dv->Refs;
}

// Kill register rx, recycle or collapse any DomainValue.
void SSEDomainFixPass::Kill(int rx) {
  assert(unsigned(rx) < NumRegs && "Invalid index");
  if (!LiveRegs || !LiveRegs[rx]) return;

  // Before killing the last reference to an open DomainValue, collapse it to
  // the first available domain.
  if (LiveRegs[rx]->Refs == 1 && !LiveRegs[rx]->collapsed())
    Collapse(LiveRegs[rx], LiveRegs[rx]->firstDomain());
  else
    SetLiveReg(rx, 0);
}

/// Force register rx into domain.
void SSEDomainFixPass::Force(int rx, unsigned domain) {
  assert(unsigned(rx) < NumRegs && "Invalid index");
  DomainValue *dv;
  if (LiveRegs && (dv = LiveRegs[rx])) {
    if (dv->collapsed())
      dv->add(domain);
    else
      Collapse(dv, domain);
  } else {
    // Set up basic collapsed DomainValue.
    dv = Pool.Alloc();
    dv->Dist = Distance;
    dv->add(domain);
    SetLiveReg(rx, dv);
  }
}

/// Collapse open DomainValue into given domain. If there are multiple
/// registers using dv, they each get a unique collapsed DomainValue.
void SSEDomainFixPass::Collapse(DomainValue *dv, unsigned domain) {
  assert(dv->compat(1u << domain) && "Cannot collapse");

  // Collapse all the instructions.
  while (!dv->Instrs.empty()) {
    MachineInstr *mi = dv->Instrs.back();
    TII->SetSSEDomain(mi, domain);
    dv->Instrs.pop_back();
  }
  dv->Mask = 1u << domain;

  // If there are multiple users, give them new, unique DomainValues.
  if (LiveRegs && dv->Refs > 1) {
    for (unsigned rx = 0; rx != NumRegs; ++rx)
      if (LiveRegs[rx] == dv) {
        DomainValue *dv2 = Pool.Alloc();
        dv2->Dist = Distance;
        dv2->add(domain);
        SetLiveReg(rx, dv2);
      }
  }
}

/// Merge - All instructions and registers in B are moved to A, and B is
/// released.
bool SSEDomainFixPass::Merge(DomainValue *A, DomainValue *B) {
  assert(!A->collapsed() && "Cannot merge into collapsed");
  assert(!B->collapsed() && "Cannot merge from collapsed");
  if (A == B)
    return true;
  if (!A->compat(B->Mask))
    return false;
  A->Mask &= B->Mask;
  A->Dist = std::max(A->Dist, B->Dist);
  A->Instrs.append(B->Instrs.begin(), B->Instrs.end());
  for (unsigned rx = 0; rx != NumRegs; ++rx)
    if (LiveRegs[rx] == B)
      SetLiveReg(rx, A);
  return true;
}

void SSEDomainFixPass::enterBasicBlock() {
  // Try to coalesce live-out registers from predecessors.
  for (MachineBasicBlock::const_livein_iterator i = MBB->livein_begin(),
         e = MBB->livein_end(); i != e; ++i) {
    int rx = RegIndex(*i);
    if (rx < 0) continue;
    for (MachineBasicBlock::const_pred_iterator pi = MBB->pred_begin(),
           pe = MBB->pred_end(); pi != pe; ++pi) {
      LiveOutMap::const_iterator fi = LiveOuts.find(*pi);
      if (fi == LiveOuts.end()) continue;
      DomainValue *pdv = fi->second[rx];
      if (!pdv) continue;
      if (!LiveRegs || !LiveRegs[rx])
        SetLiveReg(rx, pdv);
      else {
        // We have a live DomainValue from more than one predecessor.
        if (LiveRegs[rx]->collapsed()) {
          // We are already collapsed, but predecessor is not. Force him.
          if (!pdv->collapsed())
            Collapse(pdv, LiveRegs[rx]->firstDomain());
        } else {
          // Currently open, merge in predecessor.
          if (!pdv->collapsed())
            Merge(LiveRegs[rx], pdv);
          else
            Collapse(LiveRegs[rx], pdv->firstDomain());
        }
      }
    }
  }
}

// A hard instruction only works in one domain. All input registers will be
// forced into that domain.
void SSEDomainFixPass::visitHardInstr(MachineInstr *mi, unsigned domain) {
  // Collapse all uses.
  for (unsigned i = mi->getDesc().getNumDefs(),
                e = mi->getDesc().getNumOperands(); i != e; ++i) {
    MachineOperand &mo = mi->getOperand(i);
    if (!mo.isReg()) continue;
    int rx = RegIndex(mo.getReg());
    if (rx < 0) continue;
    Force(rx, domain);
  }

  // Kill all defs and force them.
  for (unsigned i = 0, e = mi->getDesc().getNumDefs(); i != e; ++i) {
    MachineOperand &mo = mi->getOperand(i);
    if (!mo.isReg()) continue;
    int rx = RegIndex(mo.getReg());
    if (rx < 0) continue;
    Kill(rx);
    Force(rx, domain);
  }
}

// A soft instruction can be changed to work in other domains given by mask.
void SSEDomainFixPass::visitSoftInstr(MachineInstr *mi, unsigned mask) {
  // Scan the explicit use operands for incoming domains.
  unsigned collmask = mask;
  SmallVector<int, 4> used;
  if (LiveRegs)
    for (unsigned i = mi->getDesc().getNumDefs(),
                  e = mi->getDesc().getNumOperands(); i != e; ++i) {
    MachineOperand &mo = mi->getOperand(i);
    if (!mo.isReg()) continue;
    int rx = RegIndex(mo.getReg());
    if (rx < 0) continue;
    if (DomainValue *dv = LiveRegs[rx]) {
      // Is it possible to use this collapsed register for free?
      if (dv->collapsed()) {
        if (unsigned m = collmask & dv->Mask)
          collmask = m;
      } else if (dv->compat(collmask))
        used.push_back(rx);
      else
        Kill(rx);
    }
  }

  // If the collapsed operands force a single domain, propagate the collapse.
  if (isPowerOf2_32(collmask)) {
    unsigned domain = CountTrailingZeros_32(collmask);
    TII->SetSSEDomain(mi, domain);
    visitHardInstr(mi, domain);
    return;
  }

  // Kill off any remaining uses that don't match collmask, and build a list of
  // incoming DomainValue that we want to merge.
  SmallVector<DomainValue*,4> doms;
  for (SmallVector<int, 4>::iterator i=used.begin(), e=used.end(); i!=e; ++i) {
    int rx = *i;
    DomainValue *dv = LiveRegs[rx];
    // This useless DomainValue could have been missed above.
    if (!dv->compat(collmask)) {
      Kill(*i);
      continue;
    }
    // sorted, uniqued insert.
    bool inserted = false;
    for (SmallVector<DomainValue*,4>::iterator i = doms.begin(), e = doms.end();
           i != e && !inserted; ++i) {
      if (dv == *i)
        inserted = true;
      else if (dv->Dist < (*i)->Dist) {
        inserted = true;
        doms.insert(i, dv);
      }
    }
    if (!inserted)
      doms.push_back(dv);
  }

  //  doms are now sorted in order of appearance. Try to merge them all, giving
  //  priority to the latest ones.
  DomainValue *dv = 0;
  while (!doms.empty()) {
    if (!dv)
      dv = doms.back();
    else if (!Merge(dv, doms.back()))
      for (SmallVector<int,4>::iterator i=used.begin(), e=used.end(); i!=e; ++i)
        if (LiveRegs[*i] == doms.back())
          Kill(*i);
    doms.pop_back();
  }

  // dv is the DomainValue we are going to use for this instruction.
  if (!dv)
    dv = Pool.Alloc();
  dv->Dist = Distance;
  dv->Mask = collmask;
  dv->Instrs.push_back(mi);

  // Finally set all defs and non-collapsed uses to dv.
  for (unsigned i = 0, e = mi->getDesc().getNumOperands(); i != e; ++i) {
    MachineOperand &mo = mi->getOperand(i);
    if (!mo.isReg()) continue;
    int rx = RegIndex(mo.getReg());
    if (rx < 0) continue;
    if (!LiveRegs || !LiveRegs[rx] || (mo.isDef() && LiveRegs[rx]!=dv)) {
      Kill(rx);
      SetLiveReg(rx, dv);
    }
  }
}

void SSEDomainFixPass::visitGenericInstr(MachineInstr *mi) {
  // Process explicit defs, kill any XMM registers redefined.
  for (unsigned i = 0, e = mi->getDesc().getNumDefs(); i != e; ++i) {
    MachineOperand &mo = mi->getOperand(i);
    if (!mo.isReg()) continue;
    int rx = RegIndex(mo.getReg());
    if (rx < 0) continue;
    Kill(rx);
  }
}

bool SSEDomainFixPass::runOnMachineFunction(MachineFunction &mf) {
  MF = &mf;
  TII = static_cast<const X86InstrInfo*>(MF->getTarget().getInstrInfo());
  TRI = MF->getTarget().getRegisterInfo();
  MBB = 0;
  LiveRegs = 0;
  Distance = 0;
  assert(NumRegs == X86::VR128RegClass.getNumRegs() && "Bad regclass");

  // If no XMM registers are used in the function, we can skip it completely.
  bool anyregs = false;
  for (TargetRegisterClass::const_iterator I = X86::VR128RegClass.begin(),
         E = X86::VR128RegClass.end(); I != E; ++I)
    if (MF->getRegInfo().isPhysRegUsed(*I)) {
      anyregs = true;
      break;
    }
  if (!anyregs) return false;

  MachineBasicBlock *Entry = MF->begin();
  SmallPtrSet<MachineBasicBlock*, 16> Visited;
  for (df_ext_iterator<MachineBasicBlock*, SmallPtrSet<MachineBasicBlock*, 16> >
         DFI = df_ext_begin(Entry, Visited), DFE = df_ext_end(Entry, Visited);
         DFI != DFE; ++DFI) {
    MBB = *DFI;
    enterBasicBlock();
    for (MachineBasicBlock::iterator I = MBB->begin(), E = MBB->end(); I != E;
        ++I) {
      MachineInstr *mi = I;
      if (mi->isDebugValue()) continue;
      ++Distance;
      std::pair<uint16_t, uint16_t> domp = TII->GetSSEDomain(mi);
      if (domp.first)
        if (domp.second)
          visitSoftInstr(mi, domp.second);
        else
          visitHardInstr(mi, domp.first);
      else if (LiveRegs)
        visitGenericInstr(mi);
    }

    // Save live registers at end of MBB - used by enterBasicBlock().
    if (LiveRegs)
      LiveOuts.insert(std::make_pair(MBB, LiveRegs));
    LiveRegs = 0;
  }

  // Clear the LiveOuts vectors. Should we also collapse any remaining
  // DomainValues?
  for (LiveOutMap::const_iterator i = LiveOuts.begin(), e = LiveOuts.end();
         i != e; ++i)
    free(i->second);
  LiveOuts.clear();
  Pool.Clear();

  return false;
}

FunctionPass *llvm::createSSEDomainFixPass() {
  return new SSEDomainFixPass();
}
