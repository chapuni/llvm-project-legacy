//===- MIRPrinter.cpp - MIR serialization format printer ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the class that prints out the LLVM IR and machine
// functions using the MIR serialization format.
//
//===----------------------------------------------------------------------===//

#include "MIRPrinter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetSubtargetInfo.h"

using namespace llvm;

namespace {

/// This structure describes how to print out stack object references.
struct FrameIndexOperand {
  std::string Name;
  unsigned ID;
  bool IsFixed;

  FrameIndexOperand(StringRef Name, unsigned ID, bool IsFixed)
      : Name(Name.str()), ID(ID), IsFixed(IsFixed) {}

  /// Return an ordinary stack object reference.
  static FrameIndexOperand create(StringRef Name, unsigned ID) {
    return FrameIndexOperand(Name, ID, /*IsFixed=*/false);
  }

  /// Return a fixed stack object reference.
  static FrameIndexOperand createFixed(unsigned ID) {
    return FrameIndexOperand("", ID, /*IsFixed=*/true);
  }
};

/// This class prints out the machine functions using the MIR serialization
/// format.
class MIRPrinter {
  raw_ostream &OS;
  DenseMap<const uint32_t *, unsigned> RegisterMaskIds;
  /// Maps from stack object indices to operand indices which will be used when
  /// printing frame index machine operands.
  DenseMap<int, FrameIndexOperand> StackObjectOperandMapping;

public:
  MIRPrinter(raw_ostream &OS) : OS(OS) {}

  void print(const MachineFunction &MF);

  void convert(yaml::MachineFunction &MF, const MachineRegisterInfo &RegInfo,
               const TargetRegisterInfo *TRI);
  void convert(yaml::MachineFrameInfo &YamlMFI, const MachineFrameInfo &MFI);
  void convert(yaml::MachineFunction &MF,
               const MachineConstantPool &ConstantPool);
  void convert(ModuleSlotTracker &MST, yaml::MachineJumpTable &YamlJTI,
               const MachineJumpTableInfo &JTI);
  void convert(ModuleSlotTracker &MST, yaml::MachineBasicBlock &YamlMBB,
               const MachineBasicBlock &MBB);
  void convertStackObjects(yaml::MachineFunction &MF,
                           const MachineFrameInfo &MFI);

private:
  void initRegisterMaskIds(const MachineFunction &MF);
};

/// This class prints out the machine instructions using the MIR serialization
/// format.
class MIPrinter {
  raw_ostream &OS;
  ModuleSlotTracker &MST;
  const DenseMap<const uint32_t *, unsigned> &RegisterMaskIds;
  const DenseMap<int, FrameIndexOperand> &StackObjectOperandMapping;

public:
  MIPrinter(raw_ostream &OS, ModuleSlotTracker &MST,
            const DenseMap<const uint32_t *, unsigned> &RegisterMaskIds,
            const DenseMap<int, FrameIndexOperand> &StackObjectOperandMapping)
      : OS(OS), MST(MST), RegisterMaskIds(RegisterMaskIds),
        StackObjectOperandMapping(StackObjectOperandMapping) {}

  void print(const MachineInstr &MI);
  void printMBBReference(const MachineBasicBlock &MBB);
  void printStackObjectReference(int FrameIndex);
  void print(const MachineOperand &Op, const TargetRegisterInfo *TRI);

  void print(const MCCFIInstruction &CFI);
};

} // end anonymous namespace

namespace llvm {
namespace yaml {

/// This struct serializes the LLVM IR module.
template <> struct BlockScalarTraits<Module> {
  static void output(const Module &Mod, void *Ctxt, raw_ostream &OS) {
    Mod.print(OS, nullptr);
  }
  static StringRef input(StringRef Str, void *Ctxt, Module &Mod) {
    llvm_unreachable("LLVM Module is supposed to be parsed separately");
    return "";
  }
};

} // end namespace yaml
} // end namespace llvm

static void printReg(unsigned Reg, raw_ostream &OS,
                     const TargetRegisterInfo *TRI) {
  // TODO: Print Stack Slots.
  if (!Reg)
    OS << '_';
  else if (TargetRegisterInfo::isVirtualRegister(Reg))
    OS << '%' << TargetRegisterInfo::virtReg2Index(Reg);
  else if (Reg < TRI->getNumRegs())
    OS << '%' << StringRef(TRI->getName(Reg)).lower();
  else
    llvm_unreachable("Can't print this kind of register yet");
}

void MIRPrinter::print(const MachineFunction &MF) {
  initRegisterMaskIds(MF);

  yaml::MachineFunction YamlMF;
  YamlMF.Name = MF.getName();
  YamlMF.Alignment = MF.getAlignment();
  YamlMF.ExposesReturnsTwice = MF.exposesReturnsTwice();
  YamlMF.HasInlineAsm = MF.hasInlineAsm();
  convert(YamlMF, MF.getRegInfo(), MF.getSubtarget().getRegisterInfo());
  convert(YamlMF.FrameInfo, *MF.getFrameInfo());
  convertStackObjects(YamlMF, *MF.getFrameInfo());
  if (const auto *ConstantPool = MF.getConstantPool())
    convert(YamlMF, *ConstantPool);

  ModuleSlotTracker MST(MF.getFunction()->getParent());
  if (const auto *JumpTableInfo = MF.getJumpTableInfo())
    convert(MST, YamlMF.JumpTableInfo, *JumpTableInfo);
  int I = 0;
  for (const auto &MBB : MF) {
    // TODO: Allow printing of non sequentially numbered MBBs.
    // This is currently needed as the basic block references get their index
    // from MBB.getNumber(), thus it should be sequential so that the parser can
    // map back to the correct MBBs when parsing the output.
    assert(MBB.getNumber() == I++ &&
           "Can't print MBBs that aren't sequentially numbered");
    (void)I;
    yaml::MachineBasicBlock YamlMBB;
    convert(MST, YamlMBB, MBB);
    YamlMF.BasicBlocks.push_back(YamlMBB);
  }
  yaml::Output Out(OS);
  Out << YamlMF;
}

void MIRPrinter::convert(yaml::MachineFunction &MF,
                         const MachineRegisterInfo &RegInfo,
                         const TargetRegisterInfo *TRI) {
  MF.IsSSA = RegInfo.isSSA();
  MF.TracksRegLiveness = RegInfo.tracksLiveness();
  MF.TracksSubRegLiveness = RegInfo.subRegLivenessEnabled();

  // Print the virtual register definitions.
  for (unsigned I = 0, E = RegInfo.getNumVirtRegs(); I < E; ++I) {
    unsigned Reg = TargetRegisterInfo::index2VirtReg(I);
    yaml::VirtualRegisterDefinition VReg;
    VReg.ID = I;
    VReg.Class =
        StringRef(TRI->getRegClassName(RegInfo.getRegClass(Reg))).lower();
    MF.VirtualRegisters.push_back(VReg);
  }
}

void MIRPrinter::convert(yaml::MachineFrameInfo &YamlMFI,
                         const MachineFrameInfo &MFI) {
  YamlMFI.IsFrameAddressTaken = MFI.isFrameAddressTaken();
  YamlMFI.IsReturnAddressTaken = MFI.isReturnAddressTaken();
  YamlMFI.HasStackMap = MFI.hasStackMap();
  YamlMFI.HasPatchPoint = MFI.hasPatchPoint();
  YamlMFI.StackSize = MFI.getStackSize();
  YamlMFI.OffsetAdjustment = MFI.getOffsetAdjustment();
  YamlMFI.MaxAlignment = MFI.getMaxAlignment();
  YamlMFI.AdjustsStack = MFI.adjustsStack();
  YamlMFI.HasCalls = MFI.hasCalls();
  YamlMFI.MaxCallFrameSize = MFI.getMaxCallFrameSize();
  YamlMFI.HasOpaqueSPAdjustment = MFI.hasOpaqueSPAdjustment();
  YamlMFI.HasVAStart = MFI.hasVAStart();
  YamlMFI.HasMustTailInVarArgFunc = MFI.hasMustTailInVarArgFunc();
}

void MIRPrinter::convertStackObjects(yaml::MachineFunction &MF,
                                     const MachineFrameInfo &MFI) {
  // Process fixed stack objects.
  unsigned ID = 0;
  for (int I = MFI.getObjectIndexBegin(); I < 0; ++I) {
    if (MFI.isDeadObjectIndex(I))
      continue;

    yaml::FixedMachineStackObject YamlObject;
    YamlObject.ID = ID;
    YamlObject.Type = MFI.isSpillSlotObjectIndex(I)
                          ? yaml::FixedMachineStackObject::SpillSlot
                          : yaml::FixedMachineStackObject::DefaultType;
    YamlObject.Offset = MFI.getObjectOffset(I);
    YamlObject.Size = MFI.getObjectSize(I);
    YamlObject.Alignment = MFI.getObjectAlignment(I);
    YamlObject.IsImmutable = MFI.isImmutableObjectIndex(I);
    YamlObject.IsAliased = MFI.isAliasedObjectIndex(I);
    MF.FixedStackObjects.push_back(YamlObject);
    StackObjectOperandMapping.insert(
        std::make_pair(I, FrameIndexOperand::createFixed(ID++)));
  }

  // Process ordinary stack objects.
  ID = 0;
  for (int I = 0, E = MFI.getObjectIndexEnd(); I < E; ++I) {
    if (MFI.isDeadObjectIndex(I))
      continue;

    yaml::MachineStackObject YamlObject;
    YamlObject.ID = ID;
    if (const auto *Alloca = MFI.getObjectAllocation(I))
      YamlObject.Name.Value =
          Alloca->hasName() ? Alloca->getName() : "<unnamed alloca>";
    YamlObject.Type = MFI.isSpillSlotObjectIndex(I)
                          ? yaml::MachineStackObject::SpillSlot
                          : MFI.isVariableSizedObjectIndex(I)
                                ? yaml::MachineStackObject::VariableSized
                                : yaml::MachineStackObject::DefaultType;
    YamlObject.Offset = MFI.getObjectOffset(I);
    YamlObject.Size = MFI.getObjectSize(I);
    YamlObject.Alignment = MFI.getObjectAlignment(I);

    MF.StackObjects.push_back(YamlObject);
    StackObjectOperandMapping.insert(std::make_pair(
        I, FrameIndexOperand::create(YamlObject.Name.Value, ID++)));
  }
}

void MIRPrinter::convert(yaml::MachineFunction &MF,
                         const MachineConstantPool &ConstantPool) {
  unsigned ID = 0;
  for (const MachineConstantPoolEntry &Constant : ConstantPool.getConstants()) {
    // TODO: Serialize target specific constant pool entries.
    if (Constant.isMachineConstantPoolEntry())
      llvm_unreachable("Can't print target specific constant pool entries yet");

    yaml::MachineConstantPoolValue YamlConstant;
    std::string Str;
    raw_string_ostream StrOS(Str);
    Constant.Val.ConstVal->printAsOperand(StrOS);
    YamlConstant.ID = ID++;
    YamlConstant.Value = StrOS.str();
    YamlConstant.Alignment = Constant.getAlignment();
    MF.Constants.push_back(YamlConstant);
  }
}

void MIRPrinter::convert(ModuleSlotTracker &MST,
                         yaml::MachineJumpTable &YamlJTI,
                         const MachineJumpTableInfo &JTI) {
  YamlJTI.Kind = JTI.getEntryKind();
  unsigned ID = 0;
  for (const auto &Table : JTI.getJumpTables()) {
    std::string Str;
    yaml::MachineJumpTable::Entry Entry;
    Entry.ID = ID++;
    for (const auto *MBB : Table.MBBs) {
      raw_string_ostream StrOS(Str);
      MIPrinter(StrOS, MST, RegisterMaskIds, StackObjectOperandMapping)
          .printMBBReference(*MBB);
      Entry.Blocks.push_back(StrOS.str());
      Str.clear();
    }
    YamlJTI.Entries.push_back(Entry);
  }
}

void MIRPrinter::convert(ModuleSlotTracker &MST,
                         yaml::MachineBasicBlock &YamlMBB,
                         const MachineBasicBlock &MBB) {
  assert(MBB.getNumber() >= 0 && "Invalid MBB number");
  YamlMBB.ID = (unsigned)MBB.getNumber();
  // TODO: Serialize unnamed BB references.
  if (const auto *BB = MBB.getBasicBlock())
    YamlMBB.Name.Value = BB->hasName() ? BB->getName() : "<unnamed bb>";
  else
    YamlMBB.Name.Value = "";
  YamlMBB.Alignment = MBB.getAlignment();
  YamlMBB.AddressTaken = MBB.hasAddressTaken();
  YamlMBB.IsLandingPad = MBB.isLandingPad();
  for (const auto *SuccMBB : MBB.successors()) {
    std::string Str;
    raw_string_ostream StrOS(Str);
    MIPrinter(StrOS, MST, RegisterMaskIds, StackObjectOperandMapping)
        .printMBBReference(*SuccMBB);
    YamlMBB.Successors.push_back(StrOS.str());
  }
  // Print the live in registers.
  const auto *TRI = MBB.getParent()->getSubtarget().getRegisterInfo();
  assert(TRI && "Expected target register info");
  for (auto I = MBB.livein_begin(), E = MBB.livein_end(); I != E; ++I) {
    std::string Str;
    raw_string_ostream StrOS(Str);
    printReg(*I, StrOS, TRI);
    YamlMBB.LiveIns.push_back(StrOS.str());
  }
  // Print the machine instructions.
  YamlMBB.Instructions.reserve(MBB.size());
  std::string Str;
  for (const auto &MI : MBB) {
    raw_string_ostream StrOS(Str);
    MIPrinter(StrOS, MST, RegisterMaskIds, StackObjectOperandMapping).print(MI);
    YamlMBB.Instructions.push_back(StrOS.str());
    Str.clear();
  }
}

void MIRPrinter::initRegisterMaskIds(const MachineFunction &MF) {
  const auto *TRI = MF.getSubtarget().getRegisterInfo();
  unsigned I = 0;
  for (const uint32_t *Mask : TRI->getRegMasks())
    RegisterMaskIds.insert(std::make_pair(Mask, I++));
}

void MIPrinter::print(const MachineInstr &MI) {
  const auto &SubTarget = MI.getParent()->getParent()->getSubtarget();
  const auto *TRI = SubTarget.getRegisterInfo();
  assert(TRI && "Expected target register info");
  const auto *TII = SubTarget.getInstrInfo();
  assert(TII && "Expected target instruction info");
  if (MI.isCFIInstruction())
    assert(MI.getNumOperands() == 1 && "Expected 1 operand in CFI instruction");

  unsigned I = 0, E = MI.getNumOperands();
  for (; I < E && MI.getOperand(I).isReg() && MI.getOperand(I).isDef() &&
         !MI.getOperand(I).isImplicit();
       ++I) {
    if (I)
      OS << ", ";
    print(MI.getOperand(I), TRI);
  }

  if (I)
    OS << " = ";
  if (MI.getFlag(MachineInstr::FrameSetup))
    OS << "frame-setup ";
  OS << TII->getName(MI.getOpcode());
  // TODO: Print the bundling instruction flags, machine mem operands.
  if (I < E)
    OS << ' ';

  bool NeedComma = false;
  for (; I < E; ++I) {
    if (NeedComma)
      OS << ", ";
    print(MI.getOperand(I), TRI);
    NeedComma = true;
  }
}

void MIPrinter::printMBBReference(const MachineBasicBlock &MBB) {
  OS << "%bb." << MBB.getNumber();
  if (const auto *BB = MBB.getBasicBlock()) {
    if (BB->hasName())
      OS << '.' << BB->getName();
  }
}

void MIPrinter::printStackObjectReference(int FrameIndex) {
  auto ObjectInfo = StackObjectOperandMapping.find(FrameIndex);
  assert(ObjectInfo != StackObjectOperandMapping.end() &&
         "Invalid frame index");
  const FrameIndexOperand &Operand = ObjectInfo->second;
  if (Operand.IsFixed) {
    OS << "%fixed-stack." << Operand.ID;
    return;
  }
  OS << "%stack." << Operand.ID;
  if (!Operand.Name.empty())
    OS << '.' << Operand.Name;
}

void MIPrinter::print(const MachineOperand &Op, const TargetRegisterInfo *TRI) {
  switch (Op.getType()) {
  case MachineOperand::MO_Register:
    // TODO: Print the other register flags.
    if (Op.isImplicit())
      OS << (Op.isDef() ? "implicit-def " : "implicit ");
    if (Op.isDead())
      OS << "dead ";
    if (Op.isKill())
      OS << "killed ";
    if (Op.isUndef())
      OS << "undef ";
    printReg(Op.getReg(), OS, TRI);
    // Print the sub register.
    if (Op.getSubReg() != 0)
      OS << ':' << TRI->getSubRegIndexName(Op.getSubReg());
    break;
  case MachineOperand::MO_Immediate:
    OS << Op.getImm();
    break;
  case MachineOperand::MO_MachineBasicBlock:
    printMBBReference(*Op.getMBB());
    break;
  case MachineOperand::MO_FrameIndex:
    printStackObjectReference(Op.getIndex());
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    OS << "%const." << Op.getIndex();
    // TODO: Print offset and target flags.
    break;
  case MachineOperand::MO_JumpTableIndex:
    OS << "%jump-table." << Op.getIndex();
    // TODO: Print target flags.
    break;
  case MachineOperand::MO_ExternalSymbol:
    OS << '$';
    printLLVMNameWithoutPrefix(OS, Op.getSymbolName());
    // TODO: Print the target flags.
    break;
  case MachineOperand::MO_GlobalAddress:
    Op.getGlobal()->printAsOperand(OS, /*PrintType=*/false, MST);
    // TODO: Print offset and target flags.
    break;
  case MachineOperand::MO_RegisterMask: {
    auto RegMaskInfo = RegisterMaskIds.find(Op.getRegMask());
    if (RegMaskInfo != RegisterMaskIds.end())
      OS << StringRef(TRI->getRegMaskNames()[RegMaskInfo->second]).lower();
    else
      llvm_unreachable("Can't print this machine register mask yet.");
    break;
  }
  case MachineOperand::MO_Metadata:
    Op.getMetadata()->printAsOperand(OS, MST);
    break;
  case MachineOperand::MO_CFIIndex: {
    const auto &MMI = Op.getParent()->getParent()->getParent()->getMMI();
    print(MMI.getFrameInstructions()[Op.getCFIIndex()]);
    break;
  }
  default:
    // TODO: Print the other machine operands.
    llvm_unreachable("Can't print this machine operand at the moment");
  }
}

void MIPrinter::print(const MCCFIInstruction &CFI) {
  switch (CFI.getOperation()) {
  case MCCFIInstruction::OpDefCfaOffset:
    OS << ".cfi_def_cfa_offset ";
    if (CFI.getLabel())
      OS << "<mcsymbol> ";
    OS << CFI.getOffset();
    break;
  default:
    // TODO: Print the other CFI Operations.
    OS << "<unserializable cfi operation>";
    break;
  }
}

void llvm::printMIR(raw_ostream &OS, const Module &M) {
  yaml::Output Out(OS);
  Out << const_cast<Module &>(M);
}

void llvm::printMIR(raw_ostream &OS, const MachineFunction &MF) {
  MIRPrinter Printer(OS);
  Printer.print(MF);
}
