//===- Thumb1RegisterInfo.cpp - Thumb-1 Register Information ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Thumb-1 implementation of the TargetRegisterInfo
// class.
//
//===----------------------------------------------------------------------===//

#include "ARM.h"
#include "ARMAddressingModes.h"
#include "ARMBaseInstrInfo.h"
#include "ARMMachineFunctionInfo.h"
#include "ARMSubtarget.h"
#include "Thumb1InstrInfo.h"
#include "Thumb1RegisterInfo.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLocation.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetFrameInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
extern cl::opt<bool> ReuseFrameIndexVals;
}

using namespace llvm;

Thumb1RegisterInfo::Thumb1RegisterInfo(const ARMBaseInstrInfo &tii,
                                       const ARMSubtarget &sti)
  : ARMBaseRegisterInfo(tii, sti) {
}

/// emitLoadConstPool - Emits a load from constpool to materialize the
/// specified immediate.
void Thumb1RegisterInfo::emitLoadConstPool(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator &MBBI,
                                           DebugLoc dl,
                                           unsigned DestReg, unsigned SubIdx,
                                           int Val,
                                           ARMCC::CondCodes Pred,
                                           unsigned PredReg) const {
  MachineFunction &MF = *MBB.getParent();
  MachineConstantPool *ConstantPool = MF.getConstantPool();
  const Constant *C = ConstantInt::get(
          Type::getInt32Ty(MBB.getParent()->getFunction()->getContext()), Val);
  unsigned Idx = ConstantPool->getConstantPoolIndex(C, 4);

  BuildMI(MBB, MBBI, dl, TII.get(ARM::tLDRcp))
          .addReg(DestReg, getDefRegState(true), SubIdx)
          .addConstantPoolIndex(Idx).addImm(Pred).addReg(PredReg);
}


/// emitThumbRegPlusImmInReg - Emits a series of instructions to materialize
/// a destreg = basereg + immediate in Thumb code. Materialize the immediate
/// in a register using mov / mvn sequences or load the immediate from a
/// constpool entry.
static
void emitThumbRegPlusImmInReg(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator &MBBI,
                              unsigned DestReg, unsigned BaseReg,
                              int NumBytes, bool CanChangeCC,
                              const TargetInstrInfo &TII,
                              const ARMBaseRegisterInfo& MRI,
                              DebugLoc dl) {
    MachineFunction &MF = *MBB.getParent();
    bool isHigh = !isARMLowRegister(DestReg) ||
                  (BaseReg != 0 && !isARMLowRegister(BaseReg));
    bool isSub = false;
    // Subtract doesn't have high register version. Load the negative value
    // if either base or dest register is a high register. Also, if do not
    // issue sub as part of the sequence if condition register is to be
    // preserved.
    if (NumBytes < 0 && !isHigh && CanChangeCC) {
      isSub = true;
      NumBytes = -NumBytes;
    }
    unsigned LdReg = DestReg;
    if (DestReg == ARM::SP) {
      assert(BaseReg == ARM::SP && "Unexpected!");
      LdReg = MF.getRegInfo().createVirtualRegister(ARM::tGPRRegisterClass);
    }

    if (NumBytes <= 255 && NumBytes >= 0)
      AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TII.get(ARM::tMOVi8), LdReg))
        .addImm(NumBytes);
    else if (NumBytes < 0 && NumBytes >= -255) {
      AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TII.get(ARM::tMOVi8), LdReg))
        .addImm(NumBytes);
      AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TII.get(ARM::tRSB), LdReg))
        .addReg(LdReg, RegState::Kill);
    } else
      MRI.emitLoadConstPool(MBB, MBBI, dl, LdReg, 0, NumBytes);

    // Emit add / sub.
    int Opc = (isSub) ? ARM::tSUBrr : (isHigh ? ARM::tADDhirr : ARM::tADDrr);
    MachineInstrBuilder MIB =
      BuildMI(MBB, MBBI, dl, TII.get(Opc), DestReg);
    if (Opc != ARM::tADDhirr)
      MIB = AddDefaultT1CC(MIB);
    if (DestReg == ARM::SP || isSub)
      MIB.addReg(BaseReg).addReg(LdReg, RegState::Kill);
    else
      MIB.addReg(LdReg).addReg(BaseReg, RegState::Kill);
    AddDefaultPred(MIB);
}

/// calcNumMI - Returns the number of instructions required to materialize
/// the specific add / sub r, c instruction.
static unsigned calcNumMI(int Opc, int ExtraOpc, unsigned Bytes,
                          unsigned NumBits, unsigned Scale) {
  unsigned NumMIs = 0;
  unsigned Chunk = ((1 << NumBits) - 1) * Scale;

  if (Opc == ARM::tADDrSPi) {
    unsigned ThisVal = (Bytes > Chunk) ? Chunk : Bytes;
    Bytes -= ThisVal;
    NumMIs++;
    NumBits = 8;
    Scale = 1;  // Followed by a number of tADDi8.
    Chunk = ((1 << NumBits) - 1) * Scale;
  }

  NumMIs += Bytes / Chunk;
  if ((Bytes % Chunk) != 0)
    NumMIs++;
  if (ExtraOpc)
    NumMIs++;
  return NumMIs;
}

/// emitThumbRegPlusImmediate - Emits a series of instructions to materialize
/// a destreg = basereg + immediate in Thumb code.
void llvm::emitThumbRegPlusImmediate(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator &MBBI,
                                     unsigned DestReg, unsigned BaseReg,
                                     int NumBytes, const TargetInstrInfo &TII,
                                     const ARMBaseRegisterInfo& MRI,
                                     DebugLoc dl) {
  bool isSub = NumBytes < 0;
  unsigned Bytes = (unsigned)NumBytes;
  if (isSub) Bytes = -NumBytes;
  bool isMul4 = (Bytes & 3) == 0;
  bool isTwoAddr = false;
  bool DstNotEqBase = false;
  unsigned NumBits = 1;
  unsigned Scale = 1;
  int Opc = 0;
  int ExtraOpc = 0;
  bool NeedCC = false;
  bool NeedPred = false;

  if (DestReg == BaseReg && BaseReg == ARM::SP) {
    assert(isMul4 && "Thumb sp inc / dec size must be multiple of 4!");
    NumBits = 7;
    Scale = 4;
    Opc = isSub ? ARM::tSUBspi : ARM::tADDspi;
    isTwoAddr = true;
  } else if (!isSub && BaseReg == ARM::SP) {
    // r1 = add sp, 403
    // =>
    // r1 = add sp, 100 * 4
    // r1 = add r1, 3
    if (!isMul4) {
      Bytes &= ~3;
      ExtraOpc = ARM::tADDi3;
    }
    NumBits = 8;
    Scale = 4;
    Opc = ARM::tADDrSPi;
  } else {
    // sp = sub sp, c
    // r1 = sub sp, c
    // r8 = sub sp, c
    if (DestReg != BaseReg)
      DstNotEqBase = true;
    NumBits = 8;
    if (DestReg == ARM::SP) {
      Opc = isSub ? ARM::tSUBspi : ARM::tADDspi;
      assert(isMul4 && "Thumb sp inc / dec size must be multiple of 4!");
      NumBits = 7;
      Scale = 4;
    } else {
      Opc = isSub ? ARM::tSUBi8 : ARM::tADDi8;
      NumBits = 8;
      NeedPred = NeedCC = true;
    }
    isTwoAddr = true;
  }

  unsigned NumMIs = calcNumMI(Opc, ExtraOpc, Bytes, NumBits, Scale);
  unsigned Threshold = (DestReg == ARM::SP) ? 3 : 2;
  if (NumMIs > Threshold) {
    // This will expand into too many instructions. Load the immediate from a
    // constpool entry.
    emitThumbRegPlusImmInReg(MBB, MBBI, DestReg, BaseReg, NumBytes, true, TII,
                             MRI, dl);
    return;
  }

  if (DstNotEqBase) {
    if (isARMLowRegister(DestReg) && isARMLowRegister(BaseReg)) {
      // If both are low registers, emit DestReg = add BaseReg, max(Imm, 7)
      unsigned Chunk = (1 << 3) - 1;
      unsigned ThisVal = (Bytes > Chunk) ? Chunk : Bytes;
      Bytes -= ThisVal;
      const TargetInstrDesc &TID = TII.get(isSub ? ARM::tSUBi3 : ARM::tADDi3);
      const MachineInstrBuilder MIB =
        AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TID, DestReg));
      AddDefaultPred(MIB.addReg(BaseReg, RegState::Kill).addImm(ThisVal));
    } else {
      BuildMI(MBB, MBBI, dl, TII.get(ARM::tMOVr), DestReg)
        .addReg(BaseReg, RegState::Kill);
    }
    BaseReg = DestReg;
  }

  unsigned Chunk = ((1 << NumBits) - 1) * Scale;
  while (Bytes) {
    unsigned ThisVal = (Bytes > Chunk) ? Chunk : Bytes;
    Bytes -= ThisVal;
    ThisVal /= Scale;
    // Build the new tADD / tSUB.
    if (isTwoAddr) {
      MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dl, TII.get(Opc), DestReg);
      if (NeedCC)
        MIB = AddDefaultT1CC(MIB);
      MIB .addReg(DestReg).addImm(ThisVal);
      if (NeedPred)
        MIB = AddDefaultPred(MIB);
    }
    else {
      bool isKill = BaseReg != ARM::SP;
      MachineInstrBuilder MIB = BuildMI(MBB, MBBI, dl, TII.get(Opc), DestReg);
      if (NeedCC)
        MIB = AddDefaultT1CC(MIB);
      MIB.addReg(BaseReg, getKillRegState(isKill)).addImm(ThisVal);
      if (NeedPred)
        MIB = AddDefaultPred(MIB);
      BaseReg = DestReg;

      if (Opc == ARM::tADDrSPi) {
        // r4 = add sp, imm
        // r4 = add r4, imm
        // ...
        NumBits = 8;
        Scale = 1;
        Chunk = ((1 << NumBits) - 1) * Scale;
        Opc = isSub ? ARM::tSUBi8 : ARM::tADDi8;
        NeedPred = NeedCC = isTwoAddr = true;
      }
    }
  }

  if (ExtraOpc) {
    const TargetInstrDesc &TID = TII.get(ExtraOpc);
    AddDefaultPred(AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TID, DestReg))
                   .addReg(DestReg, RegState::Kill)
                   .addImm(((unsigned)NumBytes) & 3));
  }
}

static void emitSPUpdate(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator &MBBI,
                         const TargetInstrInfo &TII, DebugLoc dl,
                         const Thumb1RegisterInfo &MRI,
                         int NumBytes) {
  emitThumbRegPlusImmediate(MBB, MBBI, ARM::SP, ARM::SP, NumBytes, TII,
                            MRI, dl);
}

void Thumb1RegisterInfo::
eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator I) const {
  const TargetFrameInfo *TFI = MF.getTarget().getFrameInfo();

  if (!TFI->hasReservedCallFrame(MF)) {
    // If we have alloca, convert as follows:
    // ADJCALLSTACKDOWN -> sub, sp, sp, amount
    // ADJCALLSTACKUP   -> add, sp, sp, amount
    MachineInstr *Old = I;
    DebugLoc dl = Old->getDebugLoc();
    unsigned Amount = Old->getOperand(0).getImm();
    if (Amount != 0) {
      // We need to keep the stack aligned properly.  To do this, we round the
      // amount of space needed for the outgoing arguments up to the next
      // alignment boundary.
      unsigned Align = MF.getTarget().getFrameInfo()->getStackAlignment();
      Amount = (Amount+Align-1)/Align*Align;

      // Replace the pseudo instruction with a new instruction...
      unsigned Opc = Old->getOpcode();
      if (Opc == ARM::ADJCALLSTACKDOWN || Opc == ARM::tADJCALLSTACKDOWN) {
        emitSPUpdate(MBB, I, TII, dl, *this, -Amount);
      } else {
        assert(Opc == ARM::ADJCALLSTACKUP || Opc == ARM::tADJCALLSTACKUP);
        emitSPUpdate(MBB, I, TII, dl, *this, Amount);
      }
    }
  }
  MBB.erase(I);
}

/// emitThumbConstant - Emit a series of instructions to materialize a
/// constant.
static void emitThumbConstant(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator &MBBI,
                              unsigned DestReg, int Imm,
                              const TargetInstrInfo &TII,
                              const Thumb1RegisterInfo& MRI,
                              DebugLoc dl) {
  bool isSub = Imm < 0;
  if (isSub) Imm = -Imm;

  int Chunk = (1 << 8) - 1;
  int ThisVal = (Imm > Chunk) ? Chunk : Imm;
  Imm -= ThisVal;
  AddDefaultPred(AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TII.get(ARM::tMOVi8),
                                        DestReg))
                 .addImm(ThisVal));
  if (Imm > 0)
    emitThumbRegPlusImmediate(MBB, MBBI, DestReg, DestReg, Imm, TII, MRI, dl);
  if (isSub) {
    const TargetInstrDesc &TID = TII.get(ARM::tRSB);
    AddDefaultPred(AddDefaultT1CC(BuildMI(MBB, MBBI, dl, TID, DestReg))
                   .addReg(DestReg, RegState::Kill));
  }
}

static void removeOperands(MachineInstr &MI, unsigned i) {
  unsigned Op = i;
  for (unsigned e = MI.getNumOperands(); i != e; ++i)
    MI.RemoveOperand(Op);
}

bool Thumb1RegisterInfo::
rewriteFrameIndex(MachineBasicBlock::iterator II, unsigned FrameRegIdx,
                  unsigned FrameReg, int &Offset,
                  const ARMBaseInstrInfo &TII) const {
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc dl = MI.getDebugLoc();
  unsigned Opcode = MI.getOpcode();
  const TargetInstrDesc &Desc = MI.getDesc();
  unsigned AddrMode = (Desc.TSFlags & ARMII::AddrModeMask);

  if (Opcode == ARM::tADDrSPi) {
    Offset += MI.getOperand(FrameRegIdx+1).getImm();

    // Can't use tADDrSPi if it's based off the frame pointer.
    unsigned NumBits = 0;
    unsigned Scale = 1;
    if (FrameReg != ARM::SP) {
      Opcode = ARM::tADDi3;
      MI.setDesc(TII.get(Opcode));
      NumBits = 3;
    } else {
      NumBits = 8;
      Scale = 4;
      assert((Offset & 3) == 0 &&
             "Thumb add/sub sp, #imm immediate must be multiple of 4!");
    }

    unsigned PredReg;
    if (Offset == 0 && getInstrPredicate(&MI, PredReg) == ARMCC::AL) {
      // Turn it into a move.
      MI.setDesc(TII.get(ARM::tMOVgpr2tgpr));
      MI.getOperand(FrameRegIdx).ChangeToRegister(FrameReg, false);
      // Remove offset and remaining explicit predicate operands.
      do MI.RemoveOperand(FrameRegIdx+1);
      while (MI.getNumOperands() > FrameRegIdx+1 &&
             (!MI.getOperand(FrameRegIdx+1).isReg() ||
              !MI.getOperand(FrameRegIdx+1).isImm()));
      return true;
    }

    // Common case: small offset, fits into instruction.
    unsigned Mask = (1 << NumBits) - 1;
    if (((Offset / Scale) & ~Mask) == 0) {
      // Replace the FrameIndex with sp / fp
      if (Opcode == ARM::tADDi3) {
        removeOperands(MI, FrameRegIdx);
        MachineInstrBuilder MIB(&MI);
        AddDefaultPred(AddDefaultT1CC(MIB).addReg(FrameReg)
                       .addImm(Offset / Scale));
      } else {
        MI.getOperand(FrameRegIdx).ChangeToRegister(FrameReg, false);
        MI.getOperand(FrameRegIdx+1).ChangeToImmediate(Offset / Scale);
      }
      return true;
    }

    unsigned DestReg = MI.getOperand(0).getReg();
    unsigned Bytes = (Offset > 0) ? Offset : -Offset;
    unsigned NumMIs = calcNumMI(Opcode, 0, Bytes, NumBits, Scale);
    // MI would expand into a large number of instructions. Don't try to
    // simplify the immediate.
    if (NumMIs > 2) {
      emitThumbRegPlusImmediate(MBB, II, DestReg, FrameReg, Offset, TII,
                                *this, dl);
      MBB.erase(II);
      return true;
    }

    if (Offset > 0) {
      // Translate r0 = add sp, imm to
      // r0 = add sp, 255*4
      // r0 = add r0, (imm - 255*4)
      if (Opcode == ARM::tADDi3) {
        removeOperands(MI, FrameRegIdx);
        MachineInstrBuilder MIB(&MI);
        AddDefaultPred(AddDefaultT1CC(MIB).addReg(FrameReg).addImm(Mask));
      } else {
        MI.getOperand(FrameRegIdx).ChangeToRegister(FrameReg, false);
        MI.getOperand(FrameRegIdx+1).ChangeToImmediate(Mask);
      }
      Offset = (Offset - Mask * Scale);
      MachineBasicBlock::iterator NII = llvm::next(II);
      emitThumbRegPlusImmediate(MBB, NII, DestReg, DestReg, Offset, TII,
                                *this, dl);
    } else {
      // Translate r0 = add sp, -imm to
      // r0 = -imm (this is then translated into a series of instructons)
      // r0 = add r0, sp
      emitThumbConstant(MBB, II, DestReg, Offset, TII, *this, dl);

      MI.setDesc(TII.get(ARM::tADDhirr));
      MI.getOperand(FrameRegIdx).ChangeToRegister(DestReg, false, false, true);
      MI.getOperand(FrameRegIdx+1).ChangeToRegister(FrameReg, false);
      if (Opcode == ARM::tADDi3) {
        MachineInstrBuilder MIB(&MI);
        AddDefaultPred(MIB);
      }
    }
    return true;
  } else {
    unsigned ImmIdx = 0;
    int InstrOffs = 0;
    unsigned NumBits = 0;
    unsigned Scale = 1;
    switch (AddrMode) {
    case ARMII::AddrModeT1_s: {
      ImmIdx = FrameRegIdx+1;
      InstrOffs = MI.getOperand(ImmIdx).getImm();
      NumBits = (FrameReg == ARM::SP) ? 8 : 5;
      Scale = 4;
      break;
    }
    default:
      llvm_unreachable("Unsupported addressing mode!");
      break;
    }

    Offset += InstrOffs * Scale;
    assert((Offset & (Scale-1)) == 0 && "Can't encode this offset!");

    // Common case: small offset, fits into instruction.
    MachineOperand &ImmOp = MI.getOperand(ImmIdx);
    int ImmedOffset = Offset / Scale;
    unsigned Mask = (1 << NumBits) - 1;
    if ((unsigned)Offset <= Mask * Scale) {
      // Replace the FrameIndex with sp
      MI.getOperand(FrameRegIdx).ChangeToRegister(FrameReg, false);
      ImmOp.ChangeToImmediate(ImmedOffset);
      return true;
    }

    bool isThumSpillRestore = Opcode == ARM::tRestore || Opcode == ARM::tSpill;
    if (AddrMode == ARMII::AddrModeT1_s) {
      // Thumb tLDRspi, tSTRspi. These will change to instructions that use
      // a different base register.
      NumBits = 5;
      Mask = (1 << NumBits) - 1;
    }
    // If this is a thumb spill / restore, we will be using a constpool load to
    // materialize the offset.
    if (AddrMode == ARMII::AddrModeT1_s && isThumSpillRestore)
      ImmOp.ChangeToImmediate(0);
    else {
      // Otherwise, it didn't fit. Pull in what we can to simplify the immed.
      ImmedOffset = ImmedOffset & Mask;
      ImmOp.ChangeToImmediate(ImmedOffset);
      Offset &= ~(Mask*Scale);
    }
  }
  return Offset == 0;
}

void
Thumb1RegisterInfo::resolveFrameIndex(MachineBasicBlock::iterator I,
                                      unsigned BaseReg, int64_t Offset) const {
  MachineInstr &MI = *I;
  int Off = Offset; // ARM doesn't need the general 64-bit offsets
  unsigned i = 0;

  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert(i < MI.getNumOperands() && "Instr doesn't have FrameIndex operand!");
  }
  bool Done = false;
  Done = rewriteFrameIndex(MI, i, BaseReg, Off, TII);
  assert (Done && "Unable to resolve frame index!");
}

/// saveScavengerRegister - Spill the register so it can be used by the
/// register scavenger. Return true.
bool
Thumb1RegisterInfo::saveScavengerRegister(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          MachineBasicBlock::iterator &UseMI,
                                          const TargetRegisterClass *RC,
                                          unsigned Reg) const {
  // Thumb1 can't use the emergency spill slot on the stack because
  // ldr/str immediate offsets must be positive, and if we're referencing
  // off the frame pointer (if, for example, there are alloca() calls in
  // the function, the offset will be negative. Use R12 instead since that's
  // a call clobbered register that we know won't be used in Thumb1 mode.
  DebugLoc DL;
  BuildMI(MBB, I, DL, TII.get(ARM::tMOVtgpr2gpr)).
    addReg(ARM::R12, RegState::Define).addReg(Reg, RegState::Kill);

  // The UseMI is where we would like to restore the register. If there's
  // interference with R12 before then, however, we'll need to restore it
  // before that instead and adjust the UseMI.
  bool done = false;
  for (MachineBasicBlock::iterator II = I; !done && II != UseMI ; ++II) {
    if (II->isDebugValue())
      continue;
    // If this instruction affects R12, adjust our restore point.
    for (unsigned i = 0, e = II->getNumOperands(); i != e; ++i) {
      const MachineOperand &MO = II->getOperand(i);
      if (!MO.isReg() || MO.isUndef() || !MO.getReg() ||
          TargetRegisterInfo::isVirtualRegister(MO.getReg()))
        continue;
      if (MO.getReg() == ARM::R12) {
        UseMI = II;
        done = true;
        break;
      }
    }
  }
  // Restore the register from R12
  BuildMI(MBB, UseMI, DL, TII.get(ARM::tMOVgpr2tgpr)).
    addReg(Reg, RegState::Define).addReg(ARM::R12, RegState::Kill);

  return true;
}

void
Thumb1RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                        int SPAdj, RegScavenger *RS) const {
  unsigned VReg = 0;
  unsigned i = 0;
  MachineInstr &MI = *II;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineFunction &MF = *MBB.getParent();
  const TargetFrameInfo *TFI = MF.getTarget().getFrameInfo();
  ARMFunctionInfo *AFI = MF.getInfo<ARMFunctionInfo>();
  DebugLoc dl = MI.getDebugLoc();

  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert(i < MI.getNumOperands() && "Instr doesn't have FrameIndex operand!");
  }

  unsigned FrameReg = ARM::SP;
  int FrameIndex = MI.getOperand(i).getIndex();
  int Offset = MF.getFrameInfo()->getObjectOffset(FrameIndex) +
               MF.getFrameInfo()->getStackSize() + SPAdj;

  if (AFI->isGPRCalleeSavedArea1Frame(FrameIndex))
    Offset -= AFI->getGPRCalleeSavedArea1Offset();
  else if (AFI->isGPRCalleeSavedArea2Frame(FrameIndex))
    Offset -= AFI->getGPRCalleeSavedArea2Offset();
  else if (MF.getFrameInfo()->hasVarSizedObjects()) {
    assert(SPAdj == 0 && TFI->hasFP(MF) && "Unexpected");
    // There are alloca()'s in this function, must reference off the frame
    // pointer or base pointer instead.
    if (!hasBasePointer(MF)) {
      FrameReg = getFrameRegister(MF);
      Offset -= AFI->getFramePtrSpillOffset();
    } else
      FrameReg = BasePtr;
  }

  // Special handling of dbg_value instructions.
  if (MI.isDebugValue()) {
    MI.getOperand(i).  ChangeToRegister(FrameReg, false /*isDef*/);
    MI.getOperand(i+1).ChangeToImmediate(Offset);
    return;
  }

  // Modify MI as necessary to handle as much of 'Offset' as possible
  assert(AFI->isThumbFunction() &&
         "This eliminateFrameIndex only supports Thumb1!");
  if (rewriteFrameIndex(MI, i, FrameReg, Offset, TII))
    return;

  // If we get here, the immediate doesn't fit into the instruction.  We folded
  // as much as possible above, handle the rest, providing a register that is
  // SP+LargeImm.
  assert(Offset && "This code isn't needed if offset already handled!");

  unsigned Opcode = MI.getOpcode();
  const TargetInstrDesc &Desc = MI.getDesc();

  // Remove predicate first.
  int PIdx = MI.findFirstPredOperandIdx();
  if (PIdx != -1)
    removeOperands(MI, PIdx);

  if (Desc.mayLoad()) {
    // Use the destination register to materialize sp + offset.
    unsigned TmpReg = MI.getOperand(0).getReg();
    bool UseRR = false;
    if (Opcode == ARM::tRestore) {
      if (FrameReg == ARM::SP)
        emitThumbRegPlusImmInReg(MBB, II, TmpReg, FrameReg,
                                 Offset, false, TII, *this, dl);
      else {
        emitLoadConstPool(MBB, II, dl, TmpReg, 0, Offset);
        UseRR = true;
      }
    } else {
      emitThumbRegPlusImmediate(MBB, II, TmpReg, FrameReg, Offset, TII,
                                *this, dl);
    }

    MI.setDesc(TII.get(ARM::tLDR));
    MI.getOperand(i).ChangeToRegister(TmpReg, false, false, true);
    if (UseRR)
      // Use [reg, reg] addrmode.
      MI.addOperand(MachineOperand::CreateReg(FrameReg, false));
    else  // tLDR has an extra register operand.
      MI.addOperand(MachineOperand::CreateReg(0, false));
  } else if (Desc.mayStore()) {
      VReg = MF.getRegInfo().createVirtualRegister(ARM::tGPRRegisterClass);
      bool UseRR = false;

      if (Opcode == ARM::tSpill) {
        if (FrameReg == ARM::SP)
          emitThumbRegPlusImmInReg(MBB, II, VReg, FrameReg,
                                   Offset, false, TII, *this, dl);
        else {
          emitLoadConstPool(MBB, II, dl, VReg, 0, Offset);
          UseRR = true;
        }
      } else
        emitThumbRegPlusImmediate(MBB, II, VReg, FrameReg, Offset, TII,
                                  *this, dl);
      MI.setDesc(TII.get(ARM::tSTR));
      MI.getOperand(i).ChangeToRegister(VReg, false, false, true);
      if (UseRR)  // Use [reg, reg] addrmode.
        MI.addOperand(MachineOperand::CreateReg(FrameReg, false));
      else // tSTR has an extra register operand.
        MI.addOperand(MachineOperand::CreateReg(0, false));
  } else
    assert(false && "Unexpected opcode!");

  // Add predicate back if it's needed.
  if (MI.getDesc().isPredicable()) {
    MachineInstrBuilder MIB(&MI);
    AddDefaultPred(MIB);
  }
}
