; RUN: not llvm-as -disable-output <%s 2>&1 | FileCheck %s
define void @foo() {
entry:
  call void @llvm.dbg.value(
      metadata i8* undef,
      i64 0,
      metadata !MDLocalVariable(tag: DW_TAG_arg_variable, scope: !1),
      metadata !MDExpression())
; CHECK-LABEL: llvm.dbg.value intrinsic requires a !dbg attachment
; CHECK-NEXT: call void @llvm.dbg.value({{.*}})
; CHECK-NEXT: label %entry
; CHECK-NEXT: void ()* @foo

  call void @llvm.dbg.declare(
      metadata i8* undef,
      metadata !MDLocalVariable(tag: DW_TAG_arg_variable, scope: !1),
      metadata !MDExpression())
; CHECK-LABEL: llvm.dbg.declare intrinsic requires a !dbg attachment
; CHECK-NEXT: call void @llvm.dbg.declare({{.*}})
; CHECK-NEXT: label %entry
; CHECK-NEXT: void ()* @foo

  call void @llvm.dbg.value(
      metadata i8* undef,
      i64 0,
      metadata !MDLocalVariable(tag: DW_TAG_arg_variable, scope: !1),
      metadata !MDExpression()),
    !dbg !MDLocation(scope: !2)
; CHECK-LABEL: mismatched subprogram between llvm.dbg.value variable and !dbg attachment
; CHECK-NEXT: call void @llvm.dbg.value({{[^,]+, [^,]+}}, metadata ![[VAR:[0-9]+]], {{[^,]+}}), !dbg ![[LOC:[0-9]+]]
; CHECK-NEXT: label %entry
; CHECK-NEXT: void ()* @foo
; CHECK-NEXT: ![[VAR]] = !MDLocalVariable({{.*}}scope: ![[VARSP:[0-9]+]]
; CHECK-NEXT: ![[VARSP]] = !MDSubprogram(
; CHECK-NEXT: ![[LOC]] = !MDLocation({{.*}}scope: ![[LOCSP:[0-9]+]]
; CHECK-NEXT: ![[LOCSP]] = !MDSubprogram(

  call void @llvm.dbg.declare(
      metadata i8* undef,
      metadata !MDLocalVariable(tag: DW_TAG_arg_variable, scope: !1),
      metadata !MDExpression()),
    !dbg !MDLocation(scope: !2)
; CHECK-LABEL: mismatched subprogram between llvm.dbg.declare variable and !dbg attachment
; CHECK-NEXT: call void @llvm.dbg.declare({{[^,]+}}, metadata ![[VAR:[0-9]+]], {{.*[^,]+}}), !dbg ![[LOC:[0-9]+]]
; CHECK-NEXT: label %entry
; CHECK-NEXT: void ()* @foo
; CHECK-NEXT: ![[VAR]] = !MDLocalVariable({{.*}}scope: ![[VARSP:[0-9]+]]
; CHECK-NEXT: ![[VARSP]] = !MDSubprogram(
; CHECK-NEXT: ![[LOC]] = !MDLocation({{.*}}scope: ![[LOCSP:[0-9]+]]
; CHECK-NEXT: ![[LOCSP]] = !MDSubprogram(

  ret void
}

declare void @llvm.dbg.value(metadata, i64, metadata, metadata)
declare void @llvm.dbg.declare(metadata, metadata, metadata)

!llvm.module.flags = !{!0}
!0 = !{i32 2, !"Debug Info Version", i32 3}
!1 = !MDSubprogram(name: "foo")
!2 = !MDSubprogram(name: "bar")
