; RUN: llc < %s -mtriple=x86_64-pc-linux -mattr=+sse2 | FileCheck %s

; SSE2 Logical Shift Left

define <8 x i16> @test_sllw_1(<8 x i16> %InVec) {
; CHECK-LABEL: test_sllw_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = shl <8 x i16> %InVec, <i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0>
  ret <8 x i16> %shl
}

define <8 x i16> @test_sllw_2(<8 x i16> %InVec) {
; CHECK-LABEL: test_sllw_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    paddw %xmm0, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <8 x i16> %InVec, <i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1>
  ret <8 x i16> %shl
}

define <8 x i16> @test_sllw_3(<8 x i16> %InVec) {
; CHECK-LABEL: test_sllw_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psllw $15, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <8 x i16> %InVec, <i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15>
  ret <8 x i16> %shl
}

define <4 x i32> @test_slld_1(<4 x i32> %InVec) {
; CHECK-LABEL: test_slld_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = shl <4 x i32> %InVec, <i32 0, i32 0, i32 0, i32 0>
  ret <4 x i32> %shl
}

define <4 x i32> @test_slld_2(<4 x i32> %InVec) {
; CHECK-LABEL: test_slld_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    paddd %xmm0, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <4 x i32> %InVec, <i32 1, i32 1, i32 1, i32 1>
  ret <4 x i32> %shl
}

define <4 x i32> @test_slld_3(<4 x i32> %InVec) {
; CHECK-LABEL: test_slld_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    pslld $31, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <4 x i32> %InVec, <i32 31, i32 31, i32 31, i32 31>
  ret <4 x i32> %shl
}

define <2 x i64> @test_sllq_1(<2 x i64> %InVec) {
; CHECK-LABEL: test_sllq_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = shl <2 x i64> %InVec, <i64 0, i64 0>
  ret <2 x i64> %shl
}

define <2 x i64> @test_sllq_2(<2 x i64> %InVec) {
; CHECK-LABEL: test_sllq_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    paddq %xmm0, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <2 x i64> %InVec, <i64 1, i64 1>
  ret <2 x i64> %shl
}

define <2 x i64> @test_sllq_3(<2 x i64> %InVec) {
; CHECK-LABEL: test_sllq_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psllq $63, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = shl <2 x i64> %InVec, <i64 63, i64 63>
  ret <2 x i64> %shl
}

; SSE2 Arithmetic Shift

define <8 x i16> @test_sraw_1(<8 x i16> %InVec) {
; CHECK-LABEL: test_sraw_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = ashr <8 x i16> %InVec, <i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0>
  ret <8 x i16> %shl
}

define <8 x i16> @test_sraw_2(<8 x i16> %InVec) {
; CHECK-LABEL: test_sraw_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psraw $1, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = ashr <8 x i16> %InVec, <i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1>
  ret <8 x i16> %shl
}

define <8 x i16> @test_sraw_3(<8 x i16> %InVec) {
; CHECK-LABEL: test_sraw_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psraw $15, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = ashr <8 x i16> %InVec, <i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15>
  ret <8 x i16> %shl
}

define <4 x i32> @test_srad_1(<4 x i32> %InVec) {
; CHECK-LABEL: test_srad_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = ashr <4 x i32> %InVec, <i32 0, i32 0, i32 0, i32 0>
  ret <4 x i32> %shl
}

define <4 x i32> @test_srad_2(<4 x i32> %InVec) {
; CHECK-LABEL: test_srad_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrad $1, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = ashr <4 x i32> %InVec, <i32 1, i32 1, i32 1, i32 1>
  ret <4 x i32> %shl
}

define <4 x i32> @test_srad_3(<4 x i32> %InVec) {
; CHECK-LABEL: test_srad_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrad $31, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = ashr <4 x i32> %InVec, <i32 31, i32 31, i32 31, i32 31>
  ret <4 x i32> %shl
}

; SSE Logical Shift Right

define <8 x i16> @test_srlw_1(<8 x i16> %InVec) {
; CHECK-LABEL: test_srlw_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = lshr <8 x i16> %InVec, <i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0, i16 0>
  ret <8 x i16> %shl
}

define <8 x i16> @test_srlw_2(<8 x i16> %InVec) {
; CHECK-LABEL: test_srlw_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrlw $1, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <8 x i16> %InVec, <i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1, i16 1>
  ret <8 x i16> %shl
}

define <8 x i16> @test_srlw_3(<8 x i16> %InVec) {
; CHECK-LABEL: test_srlw_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrlw $15, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <8 x i16> %InVec, <i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15, i16 15>
  ret <8 x i16> %shl
}

define <4 x i32> @test_srld_1(<4 x i32> %InVec) {
; CHECK-LABEL: test_srld_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = lshr <4 x i32> %InVec, <i32 0, i32 0, i32 0, i32 0>
  ret <4 x i32> %shl
}

define <4 x i32> @test_srld_2(<4 x i32> %InVec) {
; CHECK-LABEL: test_srld_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrld $1, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <4 x i32> %InVec, <i32 1, i32 1, i32 1, i32 1>
  ret <4 x i32> %shl
}

define <4 x i32> @test_srld_3(<4 x i32> %InVec) {
; CHECK-LABEL: test_srld_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrld $31, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <4 x i32> %InVec, <i32 31, i32 31, i32 31, i32 31>
  ret <4 x i32> %shl
}

define <2 x i64> @test_srlq_1(<2 x i64> %InVec) {
; CHECK-LABEL: test_srlq_1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    retq
entry:
  %shl = lshr <2 x i64> %InVec, <i64 0, i64 0>
  ret <2 x i64> %shl
}

define <2 x i64> @test_srlq_2(<2 x i64> %InVec) {
; CHECK-LABEL: test_srlq_2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrlq $1, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <2 x i64> %InVec, <i64 1, i64 1>
  ret <2 x i64> %shl
}

define <2 x i64> @test_srlq_3(<2 x i64> %InVec) {
; CHECK-LABEL: test_srlq_3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    psrlq $63, %xmm0
; CHECK-NEXT:    retq
entry:
  %shl = lshr <2 x i64> %InVec, <i64 63, i64 63>
  ret <2 x i64> %shl
}

define <4 x i32> @sra_sra_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: sra_sra_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrad $6, %xmm0
; CHECK-NEXT:    retq
  %sra0 = ashr <4 x i32> %x, <i32 2, i32 2, i32 2, i32 2>
  %sra1 = ashr <4 x i32> %sra0, <i32 4, i32 4, i32 4, i32 4>
  ret <4 x i32> %sra1
}

define <4 x i32> @srl_srl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: srl_srl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrld $6, %xmm0
; CHECK-NEXT:    retq
  %srl0 = lshr <4 x i32> %x, <i32 2, i32 2, i32 2, i32 2>
  %srl1 = lshr <4 x i32> %srl0, <i32 4, i32 4, i32 4, i32 4>
  ret <4 x i32> %srl1
}

define <4 x i32> @srl_shl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: srl_shl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    andps {{.*}}(%rip), %xmm0
; CHECK-NEXT:    retq
  %srl0 = shl <4 x i32> %x, <i32 4, i32 4, i32 4, i32 4>
  %srl1 = lshr <4 x i32> %srl0, <i32 4, i32 4, i32 4, i32 4>
  ret <4 x i32> %srl1
}

define <4 x i32> @srl_sra_31_v4i32(<4 x i32> %x, <4 x i32> %y) nounwind {
; CHECK-LABEL: srl_sra_31_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrld $31, %xmm0
; CHECK-NEXT:    retq
  %sra = ashr <4 x i32> %x, %y
  %srl1 = lshr <4 x i32> %sra, <i32 31, i32 31, i32 31, i32 31>
  ret <4 x i32> %srl1
}

define <4 x i32> @shl_shl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: shl_shl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    pslld $6, %xmm0
; CHECK-NEXT:    retq
  %shl0 = shl <4 x i32> %x, <i32 2, i32 2, i32 2, i32 2>
  %shl1 = shl <4 x i32> %shl0, <i32 4, i32 4, i32 4, i32 4>
  ret <4 x i32> %shl1
}

define <4 x i32> @shl_sra_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: shl_sra_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    andps {{.*}}(%rip), %xmm0
; CHECK-NEXT:    retq
  %shl0 = ashr <4 x i32> %x, <i32 4, i32 4, i32 4, i32 4>
  %shl1 = shl <4 x i32> %shl0, <i32 4, i32 4, i32 4, i32 4>
  ret <4 x i32> %shl1
}

define <4 x i32> @shl_srl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: shl_srl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    pslld $3, %xmm0
; CHECK-NEXT:    pand {{.*}}(%rip), %xmm0
; CHECK-NEXT:    retq
  %shl0 = lshr <4 x i32> %x, <i32 2, i32 2, i32 2, i32 2>
  %shl1 = shl <4 x i32> %shl0, <i32 5, i32 5, i32 5, i32 5>
  ret <4 x i32> %shl1
}

define <4 x i32> @shl_zext_srl_v4i32(<4 x i16> %x) nounwind {
; CHECK-LABEL: shl_zext_srl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    andps {{.*}}(%rip), %xmm0
; CHECK-NEXT:    andps {{.*}}(%rip), %xmm0
; CHECK-NEXT:    retq
  %srl = lshr <4 x i16> %x, <i16 2, i16 2, i16 2, i16 2>
  %zext = zext <4 x i16> %srl to <4 x i32>
  %shl = shl <4 x i32> %zext, <i32 2, i32 2, i32 2, i32 2>
  ret <4 x i32> %shl
}

define <4 x i16> @sra_trunc_srl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: sra_trunc_srl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrad $19, %xmm0
; CHECK-NEXT:    retq
  %srl = lshr <4 x i32> %x, <i32 16, i32 16, i32 16, i32 16>
  %trunc = trunc <4 x i32> %srl to <4 x i16>
  %sra = ashr <4 x i16> %trunc, <i16 3, i16 3, i16 3, i16 3>
  ret <4 x i16> %sra
}

define <4 x i32> @shl_zext_shl_v4i32(<4 x i16> %x) nounwind {
; CHECK-LABEL: shl_zext_shl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    pand {{.*}}(%rip), %xmm0
; CHECK-NEXT:    pslld $19, %xmm0
; CHECK-NEXT:    retq
  %shl0 = shl <4 x i16> %x, <i16 2, i16 2, i16 2, i16 2>
  %ext = zext <4 x i16> %shl0 to <4 x i32>
  %shl1 = shl <4 x i32> %ext, <i32 17, i32 17, i32 17, i32 17>
  ret <4 x i32> %shl1
}

define <4 x i32> @sra_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: sra_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrad $3, %xmm0
; CHECK-NEXT:    retq
  %sra = ashr <4 x i32> %x, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %sra
}

define <4 x i32> @srl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: srl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    psrld $3, %xmm0
; CHECK-NEXT:    retq
  %sra = lshr <4 x i32> %x, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %sra
}

define <4 x i32> @shl_v4i32(<4 x i32> %x) nounwind {
; CHECK-LABEL: shl_v4i32:
; CHECK:       # BB#0:
; CHECK-NEXT:    pslld $3, %xmm0
; CHECK-NEXT:    retq
  %sra = shl <4 x i32> %x, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %sra
}
