//===-- MipsSEInstrInfo.cpp - Mips32/64 Instruction Information -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Mips32/64 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "MipsSEInstrInfo.h"
#include "InstPrinter/MipsInstPrinter.h"
#include "MipsMachineFunction.h"
#include "MipsTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

MipsSEInstrInfo::MipsSEInstrInfo(MipsTargetMachine &tm)
  : MipsInstrInfo(tm,
                  tm.getRelocationModel() == Reloc::PIC_ ? Mips::B : Mips::J),
    RI(*tm.getSubtargetImpl()),
    IsN64(tm.getSubtarget<MipsSubtarget>().isABI_N64()) {}

const MipsRegisterInfo &MipsSEInstrInfo::getRegisterInfo() const {
  return RI;
}

/// isLoadFromStackSlot - If the specified machine instruction is a direct
/// load from a stack slot, return the virtual or physical register number of
/// the destination along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than loading from the stack slot.
unsigned MipsSEInstrInfo::
isLoadFromStackSlot(const MachineInstr *MI, int &FrameIndex) const
{
  unsigned Opc = MI->getOpcode();

  if ((Opc == Mips::LW)   || (Opc == Mips::LD)   ||
      (Opc == Mips::LWC1) || (Opc == Mips::LDC1) || (Opc == Mips::LDC164)) {
    if ((MI->getOperand(1).isFI()) && // is a stack slot
        (MI->getOperand(2).isImm()) &&  // the imm is zero
        (isZeroImm(MI->getOperand(2)))) {
      FrameIndex = MI->getOperand(1).getIndex();
      return MI->getOperand(0).getReg();
    }
  }

  return 0;
}

/// isStoreToStackSlot - If the specified machine instruction is a direct
/// store to a stack slot, return the virtual or physical register number of
/// the source reg along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than storing to the stack slot.
unsigned MipsSEInstrInfo::
isStoreToStackSlot(const MachineInstr *MI, int &FrameIndex) const
{
  unsigned Opc = MI->getOpcode();

  if ((Opc == Mips::SW)   || (Opc == Mips::SD)   ||
      (Opc == Mips::SWC1) || (Opc == Mips::SDC1) || (Opc == Mips::SDC164)) {
    if ((MI->getOperand(1).isFI()) && // is a stack slot
        (MI->getOperand(2).isImm()) &&  // the imm is zero
        (isZeroImm(MI->getOperand(2)))) {
      FrameIndex = MI->getOperand(1).getIndex();
      return MI->getOperand(0).getReg();
    }
  }
  return 0;
}

void MipsSEInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I, DebugLoc DL,
                                  unsigned DestReg, unsigned SrcReg,
                                  bool KillSrc) const {
  unsigned Opc = 0, ZeroReg = 0;

  if (Mips::GPR32RegClass.contains(DestReg)) { // Copy to CPU Reg.
    if (Mips::GPR32RegClass.contains(SrcReg))
      Opc = Mips::ADDu, ZeroReg = Mips::ZERO;
    else if (Mips::CCRRegClass.contains(SrcReg))
      Opc = Mips::CFC1;
    else if (Mips::FGR32RegClass.contains(SrcReg))
      Opc = Mips::MFC1;
    else if (Mips::HI32RegClass.contains(SrcReg))
      Opc = Mips::MFHI, SrcReg = 0;
    else if (Mips::LO32RegClass.contains(SrcReg))
      Opc = Mips::MFLO, SrcReg = 0;
    else if (Mips::HI32DSPRegClass.contains(SrcReg))
      Opc = Mips::MFHI_DSP;
    else if (Mips::LO32DSPRegClass.contains(SrcReg))
      Opc = Mips::MFLO_DSP;
    else if (Mips::DSPCCRegClass.contains(SrcReg)) {
      BuildMI(MBB, I, DL, get(Mips::RDDSP), DestReg).addImm(1 << 4)
        .addReg(SrcReg, RegState::Implicit | getKillRegState(KillSrc));
      return;
    }
    else if (Mips::MSACtrlRegClass.contains(SrcReg))
      Opc = Mips::CFCMSA;
  }
  else if (Mips::GPR32RegClass.contains(SrcReg)) { // Copy from CPU Reg.
    if (Mips::CCRRegClass.contains(DestReg))
      Opc = Mips::CTC1;
    else if (Mips::FGR32RegClass.contains(DestReg))
      Opc = Mips::MTC1;
    else if (Mips::HI32RegClass.contains(DestReg))
      Opc = Mips::MTHI, DestReg = 0;
    else if (Mips::LO32RegClass.contains(DestReg))
      Opc = Mips::MTLO, DestReg = 0;
    else if (Mips::HI32DSPRegClass.contains(DestReg))
      Opc = Mips::MTHI_DSP;
    else if (Mips::LO32DSPRegClass.contains(DestReg))
      Opc = Mips::MTLO_DSP;
    else if (Mips::DSPCCRegClass.contains(DestReg)) {
      BuildMI(MBB, I, DL, get(Mips::WRDSP))
        .addReg(SrcReg, getKillRegState(KillSrc)).addImm(1 << 4)
        .addReg(DestReg, RegState::ImplicitDefine);
      return;
    }
    else if (Mips::MSACtrlRegClass.contains(DestReg))
      Opc = Mips::CTCMSA;
  }
  else if (Mips::FGR32RegClass.contains(DestReg, SrcReg))
    Opc = Mips::FMOV_S;
  else if (Mips::AFGR64RegClass.contains(DestReg, SrcReg))
    Opc = Mips::FMOV_D32;
  else if (Mips::FGR64RegClass.contains(DestReg, SrcReg))
    Opc = Mips::FMOV_D64;
  else if (Mips::GPR64RegClass.contains(DestReg)) { // Copy to CPU64 Reg.
    if (Mips::GPR64RegClass.contains(SrcReg))
      Opc = Mips::DADDu, ZeroReg = Mips::ZERO_64;
    else if (Mips::HI64RegClass.contains(SrcReg))
      Opc = Mips::MFHI64, SrcReg = 0;
    else if (Mips::LO64RegClass.contains(SrcReg))
      Opc = Mips::MFLO64, SrcReg = 0;
    else if (Mips::FGR64RegClass.contains(SrcReg))
      Opc = Mips::DMFC1;
  }
  else if (Mips::GPR64RegClass.contains(SrcReg)) { // Copy from CPU64 Reg.
    if (Mips::HI64RegClass.contains(DestReg))
      Opc = Mips::MTHI64, DestReg = 0;
    else if (Mips::LO64RegClass.contains(DestReg))
      Opc = Mips::MTLO64, DestReg = 0;
    else if (Mips::FGR64RegClass.contains(DestReg))
      Opc = Mips::DMTC1;
  }

  assert(Opc && "Cannot copy registers");

  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, get(Opc));

  if (DestReg)
    MIB.addReg(DestReg, RegState::Define);

  if (SrcReg)
    MIB.addReg(SrcReg, getKillRegState(KillSrc));

  if (ZeroReg)
    MIB.addReg(ZeroReg);
}

void MipsSEInstrInfo::
storeRegToStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                unsigned SrcReg, bool isKill, int FI,
                const TargetRegisterClass *RC, const TargetRegisterInfo *TRI,
                int64_t Offset) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();
  MachineMemOperand *MMO = GetMemOperand(MBB, FI, MachineMemOperand::MOStore);

  unsigned Opc = 0;

  if (Mips::GPR32RegClass.hasSubClassEq(RC))
    Opc = Mips::SW;
  else if (Mips::GPR64RegClass.hasSubClassEq(RC))
    Opc = Mips::SD;
  else if (Mips::ACC64RegClass.hasSubClassEq(RC))
    Opc = Mips::STORE_ACC64;
  else if (Mips::ACC64DSPRegClass.hasSubClassEq(RC))
    Opc = Mips::STORE_ACC64DSP;
  else if (Mips::ACC128RegClass.hasSubClassEq(RC))
    Opc = Mips::STORE_ACC128;
  else if (Mips::DSPCCRegClass.hasSubClassEq(RC))
    Opc = Mips::STORE_CCOND_DSP;
  else if (Mips::FGR32RegClass.hasSubClassEq(RC))
    Opc = Mips::SWC1;
  else if (Mips::AFGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::SDC1;
  else if (Mips::FGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::SDC164;
  else if (RC->hasType(MVT::v16i8))
    Opc = Mips::ST_B;
  else if (RC->hasType(MVT::v8i16) || RC->hasType(MVT::v8f16))
    Opc = Mips::ST_H;
  else if (RC->hasType(MVT::v4i32) || RC->hasType(MVT::v4f32))
    Opc = Mips::ST_W;
  else if (RC->hasType(MVT::v2i64) || RC->hasType(MVT::v2f64))
    Opc = Mips::ST_D;

  assert(Opc && "Register class not handled!");
  BuildMI(MBB, I, DL, get(Opc)).addReg(SrcReg, getKillRegState(isKill))
    .addFrameIndex(FI).addImm(Offset).addMemOperand(MMO);
}

void MipsSEInstrInfo::
loadRegFromStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                 unsigned DestReg, int FI, const TargetRegisterClass *RC,
                 const TargetRegisterInfo *TRI, int64_t Offset) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();
  MachineMemOperand *MMO = GetMemOperand(MBB, FI, MachineMemOperand::MOLoad);
  unsigned Opc = 0;

  if (Mips::GPR32RegClass.hasSubClassEq(RC))
    Opc = Mips::LW;
  else if (Mips::GPR64RegClass.hasSubClassEq(RC))
    Opc = Mips::LD;
  else if (Mips::ACC64RegClass.hasSubClassEq(RC))
    Opc = Mips::LOAD_ACC64;
  else if (Mips::ACC64DSPRegClass.hasSubClassEq(RC))
    Opc = Mips::LOAD_ACC64DSP;
  else if (Mips::ACC128RegClass.hasSubClassEq(RC))
    Opc = Mips::LOAD_ACC128;
  else if (Mips::DSPCCRegClass.hasSubClassEq(RC))
    Opc = Mips::LOAD_CCOND_DSP;
  else if (Mips::FGR32RegClass.hasSubClassEq(RC))
    Opc = Mips::LWC1;
  else if (Mips::AFGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::LDC1;
  else if (Mips::FGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::LDC164;
  else if (RC->hasType(MVT::v16i8))
    Opc = Mips::LD_B;
  else if (RC->hasType(MVT::v8i16) || RC->hasType(MVT::v8f16))
    Opc = Mips::LD_H;
  else if (RC->hasType(MVT::v4i32) || RC->hasType(MVT::v4f32))
    Opc = Mips::LD_W;
  else if (RC->hasType(MVT::v2i64) || RC->hasType(MVT::v2f64))
    Opc = Mips::LD_D;

  assert(Opc && "Register class not handled!");
  BuildMI(MBB, I, DL, get(Opc), DestReg).addFrameIndex(FI).addImm(Offset)
    .addMemOperand(MMO);
}

bool MipsSEInstrInfo::expandPostRAPseudo(MachineBasicBlock::iterator MI) const {
  MachineBasicBlock &MBB = *MI->getParent();

  switch(MI->getDesc().getOpcode()) {
  default:
    return false;
  case Mips::RetRA:
    expandRetRA(MBB, MI, Mips::RET);
    break;
  case Mips::PseudoCVT_S_W:
    expandCvtFPInt(MBB, MI, Mips::CVT_S_W, Mips::MTC1, false);
    break;
  case Mips::PseudoCVT_D32_W:
    expandCvtFPInt(MBB, MI, Mips::CVT_D32_W, Mips::MTC1, false);
    break;
  case Mips::PseudoCVT_S_L:
    expandCvtFPInt(MBB, MI, Mips::CVT_S_L, Mips::DMTC1, true);
    break;
  case Mips::PseudoCVT_D64_W:
    expandCvtFPInt(MBB, MI, Mips::CVT_D64_W, Mips::MTC1, true);
    break;
  case Mips::PseudoCVT_D64_L:
    expandCvtFPInt(MBB, MI, Mips::CVT_D64_L, Mips::DMTC1, true);
    break;
  case Mips::BuildPairF64:
    expandBuildPairF64(MBB, MI, false);
    break;
  case Mips::BuildPairF64_64:
    expandBuildPairF64(MBB, MI, true);
    break;
  case Mips::ExtractElementF64:
    expandExtractElementF64(MBB, MI, false);
    break;
  case Mips::ExtractElementF64_64:
    expandExtractElementF64(MBB, MI, true);
    break;
  case Mips::MIPSeh_return32:
  case Mips::MIPSeh_return64:
    expandEhReturn(MBB, MI);
    break;
  }

  MBB.erase(MI);
  return true;
}

/// getOppositeBranchOpc - Return the inverse of the specified
/// opcode, e.g. turning BEQ to BNE.
unsigned MipsSEInstrInfo::getOppositeBranchOpc(unsigned Opc) const {
  switch (Opc) {
  default:           llvm_unreachable("Illegal opcode!");
  case Mips::BEQ:    return Mips::BNE;
  case Mips::BNE:    return Mips::BEQ;
  case Mips::BGTZ:   return Mips::BLEZ;
  case Mips::BGEZ:   return Mips::BLTZ;
  case Mips::BLTZ:   return Mips::BGEZ;
  case Mips::BLEZ:   return Mips::BGTZ;
  case Mips::BEQ64:  return Mips::BNE64;
  case Mips::BNE64:  return Mips::BEQ64;
  case Mips::BGTZ64: return Mips::BLEZ64;
  case Mips::BGEZ64: return Mips::BLTZ64;
  case Mips::BLTZ64: return Mips::BGEZ64;
  case Mips::BLEZ64: return Mips::BGTZ64;
  case Mips::BC1T:   return Mips::BC1F;
  case Mips::BC1F:   return Mips::BC1T;
  }
}

/// Adjust SP by Amount bytes.
void MipsSEInstrInfo::adjustStackPtr(unsigned SP, int64_t Amount,
                                     MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I) const {
  const MipsSubtarget &STI = TM.getSubtarget<MipsSubtarget>();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  unsigned ADDu = STI.isABI_N64() ? Mips::DADDu : Mips::ADDu;
  unsigned ADDiu = STI.isABI_N64() ? Mips::DADDiu : Mips::ADDiu;

  if (isInt<16>(Amount))// addi sp, sp, amount
    BuildMI(MBB, I, DL, get(ADDiu), SP).addReg(SP).addImm(Amount);
  else { // Expand immediate that doesn't fit in 16-bit.
    unsigned Reg = loadImmediate(Amount, MBB, I, DL, 0);
    BuildMI(MBB, I, DL, get(ADDu), SP).addReg(SP).addReg(Reg, RegState::Kill);
  }
}

/// This function generates the sequence of instructions needed to get the
/// result of adding register REG and immediate IMM.
unsigned
MipsSEInstrInfo::loadImmediate(int64_t Imm, MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator II, DebugLoc DL,
                               unsigned *NewImm) const {
  MipsAnalyzeImmediate AnalyzeImm;
  const MipsSubtarget &STI = TM.getSubtarget<MipsSubtarget>();
  MachineRegisterInfo &RegInfo = MBB.getParent()->getRegInfo();
  unsigned Size = STI.isABI_N64() ? 64 : 32;
  unsigned LUi = STI.isABI_N64() ? Mips::LUi64 : Mips::LUi;
  unsigned ZEROReg = STI.isABI_N64() ? Mips::ZERO_64 : Mips::ZERO;
  const TargetRegisterClass *RC = STI.isABI_N64() ?
    &Mips::GPR64RegClass : &Mips::GPR32RegClass;
  bool LastInstrIsADDiu = NewImm;

  const MipsAnalyzeImmediate::InstSeq &Seq =
    AnalyzeImm.Analyze(Imm, Size, LastInstrIsADDiu);
  MipsAnalyzeImmediate::InstSeq::const_iterator Inst = Seq.begin();

  assert(Seq.size() && (!LastInstrIsADDiu || (Seq.size() > 1)));

  // The first instruction can be a LUi, which is different from other
  // instructions (ADDiu, ORI and SLL) in that it does not have a register
  // operand.
  unsigned Reg = RegInfo.createVirtualRegister(RC);

  if (Inst->Opc == LUi)
    BuildMI(MBB, II, DL, get(LUi), Reg).addImm(SignExtend64<16>(Inst->ImmOpnd));
  else
    BuildMI(MBB, II, DL, get(Inst->Opc), Reg).addReg(ZEROReg)
      .addImm(SignExtend64<16>(Inst->ImmOpnd));

  // Build the remaining instructions in Seq.
  for (++Inst; Inst != Seq.end() - LastInstrIsADDiu; ++Inst)
    BuildMI(MBB, II, DL, get(Inst->Opc), Reg).addReg(Reg, RegState::Kill)
      .addImm(SignExtend64<16>(Inst->ImmOpnd));

  if (LastInstrIsADDiu)
    *NewImm = Inst->ImmOpnd;

  return Reg;
}

unsigned MipsSEInstrInfo::getAnalyzableBrOpc(unsigned Opc) const {
  return (Opc == Mips::BEQ    || Opc == Mips::BNE    || Opc == Mips::BGTZ   ||
          Opc == Mips::BGEZ   || Opc == Mips::BLTZ   || Opc == Mips::BLEZ   ||
          Opc == Mips::BEQ64  || Opc == Mips::BNE64  || Opc == Mips::BGTZ64 ||
          Opc == Mips::BGEZ64 || Opc == Mips::BLTZ64 || Opc == Mips::BLEZ64 ||
          Opc == Mips::BC1T   || Opc == Mips::BC1F   || Opc == Mips::B      ||
          Opc == Mips::J) ?
         Opc : 0;
}

void MipsSEInstrInfo::expandRetRA(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                unsigned Opc) const {
  BuildMI(MBB, I, I->getDebugLoc(), get(Opc)).addReg(Mips::RA);
}

std::pair<bool, bool>
MipsSEInstrInfo::compareOpndSize(unsigned Opc,
                                 const MachineFunction &MF) const {
  const MCInstrDesc &Desc = get(Opc);
  assert(Desc.NumOperands == 2 && "Unary instruction expected.");
  const MipsRegisterInfo *RI = &getRegisterInfo();
  unsigned DstRegSize = getRegClass(Desc, 0, RI, MF)->getSize();
  unsigned SrcRegSize = getRegClass(Desc, 1, RI, MF)->getSize();

  return std::make_pair(DstRegSize > SrcRegSize, DstRegSize < SrcRegSize);
}

void MipsSEInstrInfo::expandCvtFPInt(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     unsigned CvtOpc, unsigned MovOpc,
                                     bool IsI64) const {
  const MCInstrDesc &CvtDesc = get(CvtOpc), &MovDesc = get(MovOpc);
  const MachineOperand &Dst = I->getOperand(0), &Src = I->getOperand(1);
  unsigned DstReg = Dst.getReg(), SrcReg = Src.getReg(), TmpReg = DstReg;
  unsigned KillSrc =  getKillRegState(Src.isKill());
  DebugLoc DL = I->getDebugLoc();
  bool DstIsLarger, SrcIsLarger;

  tie(DstIsLarger, SrcIsLarger) = compareOpndSize(CvtOpc, *MBB.getParent());

  if (DstIsLarger)
    TmpReg = getRegisterInfo().getSubReg(DstReg, Mips::sub_lo);

  if (SrcIsLarger)
    DstReg = getRegisterInfo().getSubReg(DstReg, Mips::sub_lo);

  BuildMI(MBB, I, DL, MovDesc, TmpReg).addReg(SrcReg, KillSrc);
  BuildMI(MBB, I, DL, CvtDesc, DstReg).addReg(TmpReg, RegState::Kill);
}

void MipsSEInstrInfo::expandExtractElementF64(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              bool FP64) const {
  unsigned DstReg = I->getOperand(0).getReg();
  unsigned SrcReg = I->getOperand(1).getReg();
  unsigned N = I->getOperand(2).getImm();
  DebugLoc dl = I->getDebugLoc();

  assert(N < 2 && "Invalid immediate");
  unsigned SubIdx = N ? Mips::sub_hi : Mips::sub_lo;
  unsigned SubReg = getRegisterInfo().getSubReg(SrcReg, SubIdx);

  if (SubIdx == Mips::sub_hi && FP64)
    BuildMI(MBB, I, dl, get(Mips::MFHC1), DstReg).addReg(SubReg);
  else
    BuildMI(MBB, I, dl, get(Mips::MFC1), DstReg).addReg(SubReg);
}

void MipsSEInstrInfo::expandBuildPairF64(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         bool FP64) const {
  unsigned DstReg = I->getOperand(0).getReg();
  unsigned LoReg = I->getOperand(1).getReg(), HiReg = I->getOperand(2).getReg();
  const MCInstrDesc& Mtc1Tdd = get(Mips::MTC1);
  DebugLoc dl = I->getDebugLoc();
  const TargetRegisterInfo &TRI = getRegisterInfo();

  // mtc1 Lo, $fp
  // mtc1 Hi, $fp + 1
  BuildMI(MBB, I, dl, Mtc1Tdd, TRI.getSubReg(DstReg, Mips::sub_lo))
    .addReg(LoReg);

  if (FP64)
    BuildMI(MBB, I, dl, get(Mips::MTHC1), TRI.getSubReg(DstReg, Mips::sub_hi))
      .addReg(HiReg);
  else
    BuildMI(MBB, I, dl, Mtc1Tdd, TRI.getSubReg(DstReg, Mips::sub_hi))
      .addReg(HiReg);
}

void MipsSEInstrInfo::expandEhReturn(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I) const {
  // This pseudo instruction is generated as part of the lowering of
  // ISD::EH_RETURN. We convert it to a stack increment by OffsetReg, and
  // indirect jump to TargetReg
  const MipsSubtarget &STI = TM.getSubtarget<MipsSubtarget>();
  unsigned ADDU = STI.isABI_N64() ? Mips::DADDu : Mips::ADDu;
  unsigned JR = STI.isABI_N64() ? Mips::JR64 : Mips::JR;
  unsigned SP = STI.isABI_N64() ? Mips::SP_64 : Mips::SP;
  unsigned RA = STI.isABI_N64() ? Mips::RA_64 : Mips::RA;
  unsigned T9 = STI.isABI_N64() ? Mips::T9_64 : Mips::T9;
  unsigned ZERO = STI.isABI_N64() ? Mips::ZERO_64 : Mips::ZERO;
  unsigned OffsetReg = I->getOperand(0).getReg();
  unsigned TargetReg = I->getOperand(1).getReg();

  // addu $ra, $v0, $zero
  // addu $sp, $sp, $v1
  // jr   $ra
  if (TM.getRelocationModel() == Reloc::PIC_)
    BuildMI(MBB, I, I->getDebugLoc(), TM.getInstrInfo()->get(ADDU), T9)
        .addReg(TargetReg).addReg(ZERO);
  BuildMI(MBB, I, I->getDebugLoc(), TM.getInstrInfo()->get(ADDU), RA)
      .addReg(TargetReg).addReg(ZERO);
  BuildMI(MBB, I, I->getDebugLoc(), TM.getInstrInfo()->get(ADDU), SP)
      .addReg(SP).addReg(OffsetReg);
  BuildMI(MBB, I, I->getDebugLoc(), TM.getInstrInfo()->get(JR)).addReg(RA);
}

const MipsInstrInfo *llvm::createMipsSEInstrInfo(MipsTargetMachine &TM) {
  return new MipsSEInstrInfo(TM);
}
