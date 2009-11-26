; RUN: opt < %s -gvn -S | FileCheck %s

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128"
target triple = "i386-apple-darwin7"

define i32 @test1(i32* %b, i32* %c) nounwind {
entry:
	%g = alloca i32
	%t1 = icmp eq i32* %b, null
	br i1 %t1, label %bb, label %bb1

bb:
	%t2 = load i32* %c, align 4
	%t3 = add i32 %t2, 1
	store i32 %t3, i32* %g, align 4
	br label %bb2

bb1:		; preds = %entry
	%t5 = load i32* %b, align 4
	%t6 = add i32 %t5, 1
	store i32 %t6, i32* %g, align 4
	br label %bb2

bb2:		; preds = %bb1, %bb
	%c_addr.0 = phi i32* [ %g, %bb1 ], [ %c, %bb ]
	%b_addr.0 = phi i32* [ %b, %bb1 ], [ %g, %bb ]
	%cv = load i32* %c_addr.0, align 4
	%bv = load i32* %b_addr.0, align 4
; CHECK: %bv = phi i32
; CHECK: %cv = phi i32
; CHECK-NOT: load
; CHECK: ret i32
	%ret = add i32 %cv, %bv
	ret i32 %ret
}

