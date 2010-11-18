//=======- PPCFrameInfo.cpp - PPC Frame Information ------------*- C++ -*-====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the PPC implementation of TargetFrameInfo class.
//
//===----------------------------------------------------------------------===//

#include "PPCFrameInfo.h"
#include "PPCInstrInfo.h"
#include "PPCMachineFunctionInfo.h"
#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

// FIXME This disables some code that aligns the stack to a boundary bigger than
// the default (16 bytes on Darwin) when there is a stack local of greater
// alignment.  This does not currently work, because the delta between old and
// new stack pointers is added to offsets that reference incoming parameters
// after the prolog is generated, and the code that does that doesn't handle a
// variable delta.  You don't want to do that anyway; a better approach is to
// reserve another register that retains to the incoming stack pointer, and
// reference parameters relative to that.
#define ALIGN_STACK 0


/// VRRegNo - Map from a numbered VR register to its enum value.
///
static const unsigned short VRRegNo[] = {
 PPC::V0 , PPC::V1 , PPC::V2 , PPC::V3 , PPC::V4 , PPC::V5 , PPC::V6 , PPC::V7 ,
 PPC::V8 , PPC::V9 , PPC::V10, PPC::V11, PPC::V12, PPC::V13, PPC::V14, PPC::V15,
 PPC::V16, PPC::V17, PPC::V18, PPC::V19, PPC::V20, PPC::V21, PPC::V22, PPC::V23,
 PPC::V24, PPC::V25, PPC::V26, PPC::V27, PPC::V28, PPC::V29, PPC::V30, PPC::V31
};

/// RemoveVRSaveCode - We have found that this function does not need any code
/// to manipulate the VRSAVE register, even though it uses vector registers.
/// This can happen when the only registers used are known to be live in or out
/// of the function.  Remove all of the VRSAVE related code from the function.
static void RemoveVRSaveCode(MachineInstr *MI) {
  MachineBasicBlock *Entry = MI->getParent();
  MachineFunction *MF = Entry->getParent();

  // We know that the MTVRSAVE instruction immediately follows MI.  Remove it.
  MachineBasicBlock::iterator MBBI = MI;
  ++MBBI;
  assert(MBBI != Entry->end() && MBBI->getOpcode() == PPC::MTVRSAVE);
  MBBI->eraseFromParent();

  bool RemovedAllMTVRSAVEs = true;
  // See if we can find and remove the MTVRSAVE instruction from all of the
  // epilog blocks.
  for (MachineFunction::iterator I = MF->begin(), E = MF->end(); I != E; ++I) {
    // If last instruction is a return instruction, add an epilogue
    if (!I->empty() && I->back().getDesc().isReturn()) {
      bool FoundIt = false;
      for (MBBI = I->end(); MBBI != I->begin(); ) {
        --MBBI;
        if (MBBI->getOpcode() == PPC::MTVRSAVE) {
          MBBI->eraseFromParent();  // remove it.
          FoundIt = true;
          break;
        }
      }
      RemovedAllMTVRSAVEs &= FoundIt;
    }
  }

  // If we found and removed all MTVRSAVE instructions, remove the read of
  // VRSAVE as well.
  if (RemovedAllMTVRSAVEs) {
    MBBI = MI;
    assert(MBBI != Entry->begin() && "UPDATE_VRSAVE is first instr in block?");
    --MBBI;
    assert(MBBI->getOpcode() == PPC::MFVRSAVE && "VRSAVE instrs wandered?");
    MBBI->eraseFromParent();
  }

  // Finally, nuke the UPDATE_VRSAVE.
  MI->eraseFromParent();
}

// HandleVRSaveUpdate - MI is the UPDATE_VRSAVE instruction introduced by the
// instruction selector.  Based on the vector registers that have been used,
// transform this into the appropriate ORI instruction.
static void HandleVRSaveUpdate(MachineInstr *MI, const TargetInstrInfo &TII) {
  MachineFunction *MF = MI->getParent()->getParent();
  DebugLoc dl = MI->getDebugLoc();

  unsigned UsedRegMask = 0;
  for (unsigned i = 0; i != 32; ++i)
    if (MF->getRegInfo().isPhysRegUsed(VRRegNo[i]))
      UsedRegMask |= 1 << (31-i);

  // Live in and live out values already must be in the mask, so don't bother
  // marking them.
  for (MachineRegisterInfo::livein_iterator
       I = MF->getRegInfo().livein_begin(),
       E = MF->getRegInfo().livein_end(); I != E; ++I) {
    unsigned RegNo = PPCRegisterInfo::getRegisterNumbering(I->first);
    if (VRRegNo[RegNo] == I->first)        // If this really is a vector reg.
      UsedRegMask &= ~(1 << (31-RegNo));   // Doesn't need to be marked.
  }
  for (MachineRegisterInfo::liveout_iterator
       I = MF->getRegInfo().liveout_begin(),
       E = MF->getRegInfo().liveout_end(); I != E; ++I) {
    unsigned RegNo = PPCRegisterInfo::getRegisterNumbering(*I);
    if (VRRegNo[RegNo] == *I)              // If this really is a vector reg.
      UsedRegMask &= ~(1 << (31-RegNo));   // Doesn't need to be marked.
  }

  // If no registers are used, turn this into a copy.
  if (UsedRegMask == 0) {
    // Remove all VRSAVE code.
    RemoveVRSaveCode(MI);
    return;
  }

  unsigned SrcReg = MI->getOperand(1).getReg();
  unsigned DstReg = MI->getOperand(0).getReg();

  if ((UsedRegMask & 0xFFFF) == UsedRegMask) {
    if (DstReg != SrcReg)
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORI), DstReg)
        .addReg(SrcReg)
        .addImm(UsedRegMask);
    else
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORI), DstReg)
        .addReg(SrcReg, RegState::Kill)
        .addImm(UsedRegMask);
  } else if ((UsedRegMask & 0xFFFF0000) == UsedRegMask) {
    if (DstReg != SrcReg)
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORIS), DstReg)
        .addReg(SrcReg)
        .addImm(UsedRegMask >> 16);
    else
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORIS), DstReg)
        .addReg(SrcReg, RegState::Kill)
        .addImm(UsedRegMask >> 16);
  } else {
    if (DstReg != SrcReg)
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORIS), DstReg)
        .addReg(SrcReg)
        .addImm(UsedRegMask >> 16);
    else
      BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORIS), DstReg)
        .addReg(SrcReg, RegState::Kill)
        .addImm(UsedRegMask >> 16);

    BuildMI(*MI->getParent(), MI, dl, TII.get(PPC::ORI), DstReg)
      .addReg(DstReg, RegState::Kill)
      .addImm(UsedRegMask & 0xFFFF);
  }

  // Remove the old UPDATE_VRSAVE instruction.
  MI->eraseFromParent();
}

/// determineFrameLayout - Determine the size of the frame and maximum call
/// frame size.
void PPCFrameInfo::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo *MFI = MF.getFrameInfo();

  // Get the number of bytes to allocate from the FrameInfo
  unsigned FrameSize = MFI->getStackSize();

  // Get the alignments provided by the target, and the maximum alignment
  // (if any) of the fixed frame objects.
  unsigned MaxAlign = MFI->getMaxAlignment();
  unsigned TargetAlign = MF.getTarget().getFrameInfo()->getStackAlignment();
  unsigned AlignMask = TargetAlign - 1;  //

  // If we are a leaf function, and use up to 224 bytes of stack space,
  // don't have a frame pointer, calls, or dynamic alloca then we do not need
  // to adjust the stack pointer (we fit in the Red Zone).
  bool DisableRedZone = MF.getFunction()->hasFnAttr(Attribute::NoRedZone);
  // FIXME SVR4 The 32-bit SVR4 ABI has no red zone.
  if (!DisableRedZone &&
      FrameSize <= 224 &&                          // Fits in red zone.
      !MFI->hasVarSizedObjects() &&                // No dynamic alloca.
      !MFI->adjustsStack() &&                      // No calls.
      (!ALIGN_STACK || MaxAlign <= TargetAlign)) { // No special alignment.
    // No need for frame
    MFI->setStackSize(0);
    return;
  }

  // Get the maximum call frame size of all the calls.
  unsigned maxCallFrameSize = MFI->getMaxCallFrameSize();

  // Maximum call frame needs to be at least big enough for linkage and 8 args.
  unsigned minCallFrameSize = getMinCallFrameSize(Subtarget.isPPC64(),
                                                  Subtarget.isDarwinABI());
  maxCallFrameSize = std::max(maxCallFrameSize, minCallFrameSize);

  // If we have dynamic alloca then maxCallFrameSize needs to be aligned so
  // that allocations will be aligned.
  if (MFI->hasVarSizedObjects())
    maxCallFrameSize = (maxCallFrameSize + AlignMask) & ~AlignMask;

  // Update maximum call frame size.
  MFI->setMaxCallFrameSize(maxCallFrameSize);

  // Include call frame size in total.
  FrameSize += maxCallFrameSize;

  // Make sure the frame is aligned.
  FrameSize = (FrameSize + AlignMask) & ~AlignMask;

  // Update frame info.
  MFI->setStackSize(FrameSize);
}

// hasFP - Return true if the specified function actually has a dedicated frame
// pointer register.
bool PPCFrameInfo::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo *MFI = MF.getFrameInfo();

  // Naked functions have no stack frame pushed, so we don't have a frame
  // pointer.
  if (MF.getFunction()->hasFnAttr(Attribute::Naked))
    return false;

  return DisableFramePointerElim(MF) || MFI->hasVarSizedObjects() ||
    (GuaranteedTailCallOpt && MF.getInfo<PPCFunctionInfo>()->hasFastCall());
}


void PPCFrameInfo::emitPrologue(MachineFunction &MF) const {
  MachineBasicBlock &MBB = MF.front();   // Prolog goes in entry BB
  MachineBasicBlock::iterator MBBI = MBB.begin();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  const PPCInstrInfo &TII =
    *static_cast<const PPCInstrInfo*>(MF.getTarget().getInstrInfo());

  MachineModuleInfo &MMI = MF.getMMI();
  DebugLoc dl;
  bool needsFrameMoves = MMI.hasDebugInfo() ||
       !MF.getFunction()->doesNotThrow() ||
       UnwindTablesMandatory;

  // Prepare for frame info.
  MCSymbol *FrameLabel = 0;

  // Scan the prolog, looking for an UPDATE_VRSAVE instruction.  If we find it,
  // process it.
  for (unsigned i = 0; MBBI != MBB.end(); ++i, ++MBBI) {
    if (MBBI->getOpcode() == PPC::UPDATE_VRSAVE) {
      HandleVRSaveUpdate(MBBI, TII);
      break;
    }
  }

  // Move MBBI back to the beginning of the function.
  MBBI = MBB.begin();

  // Work out frame sizes.
  determineFrameLayout(MF);
  unsigned FrameSize = MFI->getStackSize();

  int NegFrameSize = -FrameSize;

  // Get processor type.
  bool isPPC64 = Subtarget.isPPC64();
  // Get operating system
  bool isDarwinABI = Subtarget.isDarwinABI();
  // Check if the link register (LR) must be saved.
  PPCFunctionInfo *FI = MF.getInfo<PPCFunctionInfo>();
  bool MustSaveLR = FI->mustSaveLR();
  // Do we have a frame pointer for this function?
  bool HasFP = hasFP(MF) && FrameSize;

  int LROffset = PPCFrameInfo::getReturnSaveOffset(isPPC64, isDarwinABI);

  int FPOffset = 0;
  if (HasFP) {
    if (Subtarget.isSVR4ABI()) {
      MachineFrameInfo *FFI = MF.getFrameInfo();
      int FPIndex = FI->getFramePointerSaveIndex();
      assert(FPIndex && "No Frame Pointer Save Slot!");
      FPOffset = FFI->getObjectOffset(FPIndex);
    } else {
      FPOffset = PPCFrameInfo::getFramePointerSaveOffset(isPPC64, isDarwinABI);
    }
  }

  if (isPPC64) {
    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::MFLR8), PPC::X0);

    if (HasFP)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STD))
        .addReg(PPC::X31)
        .addImm(FPOffset/4)
        .addReg(PPC::X1);

    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STD))
        .addReg(PPC::X0)
        .addImm(LROffset / 4)
        .addReg(PPC::X1);
  } else {
    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::MFLR), PPC::R0);

    if (HasFP)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STW))
        .addReg(PPC::R31)
        .addImm(FPOffset)
        .addReg(PPC::R1);

    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STW))
        .addReg(PPC::R0)
        .addImm(LROffset)
        .addReg(PPC::R1);
  }

  // Skip if a leaf routine.
  if (!FrameSize) return;

  // Get stack alignments.
  unsigned TargetAlign = MF.getTarget().getFrameInfo()->getStackAlignment();
  unsigned MaxAlign = MFI->getMaxAlignment();

  // Adjust stack pointer: r1 += NegFrameSize.
  // If there is a preferred stack alignment, align R1 now
  if (!isPPC64) {
    // PPC32.
    if (ALIGN_STACK && MaxAlign > TargetAlign) {
      assert(isPowerOf2_32(MaxAlign) && isInt<16>(MaxAlign) &&
             "Invalid alignment!");
      assert(isInt<16>(NegFrameSize) && "Unhandled stack size and alignment!");

      BuildMI(MBB, MBBI, dl, TII.get(PPC::RLWINM), PPC::R0)
        .addReg(PPC::R1)
        .addImm(0)
        .addImm(32 - Log2_32(MaxAlign))
        .addImm(31);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::SUBFIC) ,PPC::R0)
        .addReg(PPC::R0, RegState::Kill)
        .addImm(NegFrameSize);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STWUX))
        .addReg(PPC::R1)
        .addReg(PPC::R1)
        .addReg(PPC::R0);
    } else if (isInt<16>(NegFrameSize)) {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STWU), PPC::R1)
        .addReg(PPC::R1)
        .addImm(NegFrameSize)
        .addReg(PPC::R1);
    } else {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LIS), PPC::R0)
        .addImm(NegFrameSize >> 16);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::ORI), PPC::R0)
        .addReg(PPC::R0, RegState::Kill)
        .addImm(NegFrameSize & 0xFFFF);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STWUX))
        .addReg(PPC::R1)
        .addReg(PPC::R1)
        .addReg(PPC::R0);
    }
  } else {    // PPC64.
    if (ALIGN_STACK && MaxAlign > TargetAlign) {
      assert(isPowerOf2_32(MaxAlign) && isInt<16>(MaxAlign) &&
             "Invalid alignment!");
      assert(isInt<16>(NegFrameSize) && "Unhandled stack size and alignment!");

      BuildMI(MBB, MBBI, dl, TII.get(PPC::RLDICL), PPC::X0)
        .addReg(PPC::X1)
        .addImm(0)
        .addImm(64 - Log2_32(MaxAlign));
      BuildMI(MBB, MBBI, dl, TII.get(PPC::SUBFIC8), PPC::X0)
        .addReg(PPC::X0)
        .addImm(NegFrameSize);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STDUX))
        .addReg(PPC::X1)
        .addReg(PPC::X1)
        .addReg(PPC::X0);
    } else if (isInt<16>(NegFrameSize)) {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STDU), PPC::X1)
        .addReg(PPC::X1)
        .addImm(NegFrameSize / 4)
        .addReg(PPC::X1);
    } else {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LIS8), PPC::X0)
        .addImm(NegFrameSize >> 16);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::ORI8), PPC::X0)
        .addReg(PPC::X0, RegState::Kill)
        .addImm(NegFrameSize & 0xFFFF);
      BuildMI(MBB, MBBI, dl, TII.get(PPC::STDUX))
        .addReg(PPC::X1)
        .addReg(PPC::X1)
        .addReg(PPC::X0);
    }
  }

  std::vector<MachineMove> &Moves = MMI.getFrameMoves();

  // Add the "machine moves" for the instructions we generated above, but in
  // reverse order.
  if (needsFrameMoves) {
    // Mark effective beginning of when frame pointer becomes valid.
    FrameLabel = MMI.getContext().CreateTempSymbol();
    BuildMI(MBB, MBBI, dl, TII.get(PPC::PROLOG_LABEL)).addSym(FrameLabel);

    // Show update of SP.
    if (NegFrameSize) {
      MachineLocation SPDst(MachineLocation::VirtualFP);
      MachineLocation SPSrc(MachineLocation::VirtualFP, NegFrameSize);
      Moves.push_back(MachineMove(FrameLabel, SPDst, SPSrc));
    } else {
      MachineLocation SP(isPPC64 ? PPC::X31 : PPC::R31);
      Moves.push_back(MachineMove(FrameLabel, SP, SP));
    }

    if (HasFP) {
      MachineLocation FPDst(MachineLocation::VirtualFP, FPOffset);
      MachineLocation FPSrc(isPPC64 ? PPC::X31 : PPC::R31);
      Moves.push_back(MachineMove(FrameLabel, FPDst, FPSrc));
    }

    if (MustSaveLR) {
      MachineLocation LRDst(MachineLocation::VirtualFP, LROffset);
      MachineLocation LRSrc(isPPC64 ? PPC::LR8 : PPC::LR);
      Moves.push_back(MachineMove(FrameLabel, LRDst, LRSrc));
    }
  }

  MCSymbol *ReadyLabel = 0;

  // If there is a frame pointer, copy R1 into R31
  if (HasFP) {
    if (!isPPC64) {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::OR), PPC::R31)
        .addReg(PPC::R1)
        .addReg(PPC::R1);
    } else {
      BuildMI(MBB, MBBI, dl, TII.get(PPC::OR8), PPC::X31)
        .addReg(PPC::X1)
        .addReg(PPC::X1);
    }

    if (needsFrameMoves) {
      ReadyLabel = MMI.getContext().CreateTempSymbol();

      // Mark effective beginning of when frame pointer is ready.
      BuildMI(MBB, MBBI, dl, TII.get(PPC::PROLOG_LABEL)).addSym(ReadyLabel);

      MachineLocation FPDst(HasFP ? (isPPC64 ? PPC::X31 : PPC::R31) :
                                    (isPPC64 ? PPC::X1 : PPC::R1));
      MachineLocation FPSrc(MachineLocation::VirtualFP);
      Moves.push_back(MachineMove(ReadyLabel, FPDst, FPSrc));
    }
  }

  if (needsFrameMoves) {
    MCSymbol *Label = HasFP ? ReadyLabel : FrameLabel;

    // Add callee saved registers to move list.
    const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();
    for (unsigned I = 0, E = CSI.size(); I != E; ++I) {
      int Offset = MFI->getObjectOffset(CSI[I].getFrameIdx());
      unsigned Reg = CSI[I].getReg();
      if (Reg == PPC::LR || Reg == PPC::LR8 || Reg == PPC::RM) continue;
      MachineLocation CSDst(MachineLocation::VirtualFP, Offset);
      MachineLocation CSSrc(Reg);
      Moves.push_back(MachineMove(Label, CSDst, CSSrc));
    }
  }
}

void PPCFrameInfo::emitEpilogue(MachineFunction &MF,
                                MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = prior(MBB.end());
  const PPCInstrInfo &TII =
    *static_cast<const PPCInstrInfo*>(MF.getTarget().getInstrInfo());

  unsigned RetOpcode = MBBI->getOpcode();
  DebugLoc dl;

  assert((RetOpcode == PPC::BLR ||
          RetOpcode == PPC::TCRETURNri ||
          RetOpcode == PPC::TCRETURNdi ||
          RetOpcode == PPC::TCRETURNai ||
          RetOpcode == PPC::TCRETURNri8 ||
          RetOpcode == PPC::TCRETURNdi8 ||
          RetOpcode == PPC::TCRETURNai8) &&
         "Can only insert epilog into returning blocks");

  // Get alignment info so we know how to restore r1
  const MachineFrameInfo *MFI = MF.getFrameInfo();
  unsigned TargetAlign = MF.getTarget().getFrameInfo()->getStackAlignment();
  unsigned MaxAlign = MFI->getMaxAlignment();

  // Get the number of bytes allocated from the FrameInfo.
  int FrameSize = MFI->getStackSize();

  // Get processor type.
  bool isPPC64 = Subtarget.isPPC64();
  // Get operating system
  bool isDarwinABI = Subtarget.isDarwinABI();
  // Check if the link register (LR) has been saved.
  PPCFunctionInfo *FI = MF.getInfo<PPCFunctionInfo>();
  bool MustSaveLR = FI->mustSaveLR();
  // Do we have a frame pointer for this function?
  bool HasFP = hasFP(MF) && FrameSize;

  int LROffset = PPCFrameInfo::getReturnSaveOffset(isPPC64, isDarwinABI);

  int FPOffset = 0;
  if (HasFP) {
    if (Subtarget.isSVR4ABI()) {
      MachineFrameInfo *FFI = MF.getFrameInfo();
      int FPIndex = FI->getFramePointerSaveIndex();
      assert(FPIndex && "No Frame Pointer Save Slot!");
      FPOffset = FFI->getObjectOffset(FPIndex);
    } else {
      FPOffset = PPCFrameInfo::getFramePointerSaveOffset(isPPC64, isDarwinABI);
    }
  }

  bool UsesTCRet =  RetOpcode == PPC::TCRETURNri ||
    RetOpcode == PPC::TCRETURNdi ||
    RetOpcode == PPC::TCRETURNai ||
    RetOpcode == PPC::TCRETURNri8 ||
    RetOpcode == PPC::TCRETURNdi8 ||
    RetOpcode == PPC::TCRETURNai8;

  if (UsesTCRet) {
    int MaxTCRetDelta = FI->getTailCallSPDelta();
    MachineOperand &StackAdjust = MBBI->getOperand(1);
    assert(StackAdjust.isImm() && "Expecting immediate value.");
    // Adjust stack pointer.
    int StackAdj = StackAdjust.getImm();
    int Delta = StackAdj - MaxTCRetDelta;
    assert((Delta >= 0) && "Delta must be positive");
    if (MaxTCRetDelta>0)
      FrameSize += (StackAdj +Delta);
    else
      FrameSize += StackAdj;
  }

  if (FrameSize) {
    // The loaded (or persistent) stack pointer value is offset by the 'stwu'
    // on entry to the function.  Add this offset back now.
    if (!isPPC64) {
      // If this function contained a fastcc call and GuaranteedTailCallOpt is
      // enabled (=> hasFastCall()==true) the fastcc call might contain a tail
      // call which invalidates the stack pointer value in SP(0). So we use the
      // value of R31 in this case.
      if (FI->hasFastCall() && isInt<16>(FrameSize)) {
        assert(hasFP(MF) && "Expecting a valid the frame pointer.");
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADDI), PPC::R1)
          .addReg(PPC::R31).addImm(FrameSize);
      } else if(FI->hasFastCall()) {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::LIS), PPC::R0)
          .addImm(FrameSize >> 16);
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ORI), PPC::R0)
          .addReg(PPC::R0, RegState::Kill)
          .addImm(FrameSize & 0xFFFF);
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADD4))
          .addReg(PPC::R1)
          .addReg(PPC::R31)
          .addReg(PPC::R0);
      } else if (isInt<16>(FrameSize) &&
                 (!ALIGN_STACK || TargetAlign >= MaxAlign) &&
                 !MFI->hasVarSizedObjects()) {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADDI), PPC::R1)
          .addReg(PPC::R1).addImm(FrameSize);
      } else {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::LWZ),PPC::R1)
          .addImm(0).addReg(PPC::R1);
      }
    } else {
      if (FI->hasFastCall() && isInt<16>(FrameSize)) {
        assert(hasFP(MF) && "Expecting a valid the frame pointer.");
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADDI8), PPC::X1)
          .addReg(PPC::X31).addImm(FrameSize);
      } else if(FI->hasFastCall()) {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::LIS8), PPC::X0)
          .addImm(FrameSize >> 16);
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ORI8), PPC::X0)
          .addReg(PPC::X0, RegState::Kill)
          .addImm(FrameSize & 0xFFFF);
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADD8))
          .addReg(PPC::X1)
          .addReg(PPC::X31)
          .addReg(PPC::X0);
      } else if (isInt<16>(FrameSize) && TargetAlign >= MaxAlign &&
            !MFI->hasVarSizedObjects()) {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::ADDI8), PPC::X1)
           .addReg(PPC::X1).addImm(FrameSize);
      } else {
        BuildMI(MBB, MBBI, dl, TII.get(PPC::LD), PPC::X1)
           .addImm(0).addReg(PPC::X1);
      }
    }
  }

  if (isPPC64) {
    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LD), PPC::X0)
        .addImm(LROffset/4).addReg(PPC::X1);

    if (HasFP)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LD), PPC::X31)
        .addImm(FPOffset/4).addReg(PPC::X1);

    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::MTLR8)).addReg(PPC::X0);
  } else {
    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LWZ), PPC::R0)
          .addImm(LROffset).addReg(PPC::R1);

    if (HasFP)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::LWZ), PPC::R31)
          .addImm(FPOffset).addReg(PPC::R1);

    if (MustSaveLR)
      BuildMI(MBB, MBBI, dl, TII.get(PPC::MTLR)).addReg(PPC::R0);
  }

  // Callee pop calling convention. Pop parameter/linkage area. Used for tail
  // call optimization
  if (GuaranteedTailCallOpt && RetOpcode == PPC::BLR &&
      MF.getFunction()->getCallingConv() == CallingConv::Fast) {
     PPCFunctionInfo *FI = MF.getInfo<PPCFunctionInfo>();
     unsigned CallerAllocatedAmt = FI->getMinReservedArea();
     unsigned StackReg = isPPC64 ? PPC::X1 : PPC::R1;
     unsigned FPReg = isPPC64 ? PPC::X31 : PPC::R31;
     unsigned TmpReg = isPPC64 ? PPC::X0 : PPC::R0;
     unsigned ADDIInstr = isPPC64 ? PPC::ADDI8 : PPC::ADDI;
     unsigned ADDInstr = isPPC64 ? PPC::ADD8 : PPC::ADD4;
     unsigned LISInstr = isPPC64 ? PPC::LIS8 : PPC::LIS;
     unsigned ORIInstr = isPPC64 ? PPC::ORI8 : PPC::ORI;

     if (CallerAllocatedAmt && isInt<16>(CallerAllocatedAmt)) {
       BuildMI(MBB, MBBI, dl, TII.get(ADDIInstr), StackReg)
         .addReg(StackReg).addImm(CallerAllocatedAmt);
     } else {
       BuildMI(MBB, MBBI, dl, TII.get(LISInstr), TmpReg)
          .addImm(CallerAllocatedAmt >> 16);
       BuildMI(MBB, MBBI, dl, TII.get(ORIInstr), TmpReg)
          .addReg(TmpReg, RegState::Kill)
          .addImm(CallerAllocatedAmt & 0xFFFF);
       BuildMI(MBB, MBBI, dl, TII.get(ADDInstr))
          .addReg(StackReg)
          .addReg(FPReg)
          .addReg(TmpReg);
     }
  } else if (RetOpcode == PPC::TCRETURNdi) {
    MBBI = prior(MBB.end());
    MachineOperand &JumpTarget = MBBI->getOperand(0);
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILB)).
      addGlobalAddress(JumpTarget.getGlobal(), JumpTarget.getOffset());
  } else if (RetOpcode == PPC::TCRETURNri) {
    MBBI = prior(MBB.end());
    assert(MBBI->getOperand(0).isReg() && "Expecting register operand.");
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILBCTR));
  } else if (RetOpcode == PPC::TCRETURNai) {
    MBBI = prior(MBB.end());
    MachineOperand &JumpTarget = MBBI->getOperand(0);
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILBA)).addImm(JumpTarget.getImm());
  } else if (RetOpcode == PPC::TCRETURNdi8) {
    MBBI = prior(MBB.end());
    MachineOperand &JumpTarget = MBBI->getOperand(0);
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILB8)).
      addGlobalAddress(JumpTarget.getGlobal(), JumpTarget.getOffset());
  } else if (RetOpcode == PPC::TCRETURNri8) {
    MBBI = prior(MBB.end());
    assert(MBBI->getOperand(0).isReg() && "Expecting register operand.");
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILBCTR8));
  } else if (RetOpcode == PPC::TCRETURNai8) {
    MBBI = prior(MBB.end());
    MachineOperand &JumpTarget = MBBI->getOperand(0);
    BuildMI(MBB, MBBI, dl, TII.get(PPC::TAILBA8)).addImm(JumpTarget.getImm());
  }
}

void PPCFrameInfo::getInitialFrameState(std::vector<MachineMove> &Moves) const {
  // Initial state of the frame pointer is R1.
  MachineLocation Dst(MachineLocation::VirtualFP);
  MachineLocation Src(PPC::R1, 0);
  Moves.push_back(MachineMove(0, Dst, Src));
}
