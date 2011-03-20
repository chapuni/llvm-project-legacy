; RUN: llc  %s -mtriple=armv7-linux-gnueabi -filetype=obj -o - | \
; RUN:    elf-dump --dump-section-data | FileCheck  -check-prefix=OBJ %s
; RUN: llc  %s -mtriple=armv7-linux-gnueabi -o - | \
; RUN:    FileCheck  -check-prefix=ASM %s


@dummy = internal global i32 666
@array00 = internal global [20 x i32] zeroinitializer
@sum = internal global i32 55
@STRIDE = internal global i32 8

; ASM:          .type   array00,%object         @ @array00
; ASM-NEXT:     .lcomm  array00,80              @ @array00
; ASM-NEXT:     .type   _MergedGlobals,%object  @ @_MergedGlobals



; OBJ:          Section 0x00000004
; OBJ-NEXT:     '.bss'

; OBJ:          'array00'
; OBJ-NEXT:     'st_value', 0x00000000
; OBJ-NEXT:     'st_size', 0x00000050
; OBJ-NEXT:     'st_bind', 0x00000000
; OBJ-NEXT:     'st_type', 0x00000001
; OBJ-NEXT:     'st_other', 0x00000000
; OBJ-NEXT:     'st_shndx', 0x00000004

define i32 @main(i32 %argc) nounwind {
  %1 = load i32* @sum, align 4
  %2 = getelementptr  [20 x i32]* @array00, i32 0, i32 %argc
  %3 = load i32* %2, align 4
  %4 = add i32 %1, %3
  ret i32 %4;
}
