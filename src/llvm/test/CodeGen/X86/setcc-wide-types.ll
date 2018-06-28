; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -mattr=sse2 | FileCheck %s --check-prefix=SSE2
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -mattr=avx2 | FileCheck %s --check-prefix=AVX2

; Equality checks of 128/256-bit values can use PMOVMSK or PTEST to avoid scalarization.

define i32 @ne_i128(<2 x i64> %x, <2 x i64> %y) {
; SSE2-LABEL: ne_i128:
; SSE2:       # BB#0:
; SSE2-NEXT:    pcmpeqb %xmm1, %xmm0
; SSE2-NEXT:    pmovmskb %xmm0, %ecx
; SSE2-NEXT:    xorl %eax, %eax
; SSE2-NEXT:    cmpl $65535, %ecx # imm = 0xFFFF
; SSE2-NEXT:    setne %al
; SSE2-NEXT:    retq
;
; AVX2-LABEL: ne_i128:
; AVX2:       # BB#0:
; AVX2-NEXT:    vpcmpeqb %xmm1, %xmm0, %xmm0
; AVX2-NEXT:    vpmovmskb %xmm0, %ecx
; AVX2-NEXT:    xorl %eax, %eax
; AVX2-NEXT:    cmpl $65535, %ecx # imm = 0xFFFF
; AVX2-NEXT:    setne %al
; AVX2-NEXT:    retq
  %bcx = bitcast <2 x i64> %x to i128
  %bcy = bitcast <2 x i64> %y to i128
  %cmp = icmp ne i128 %bcx, %bcy
  %zext = zext i1 %cmp to i32
  ret i32 %zext
}

define i32 @eq_i128(<2 x i64> %x, <2 x i64> %y) {
; SSE2-LABEL: eq_i128:
; SSE2:       # BB#0:
; SSE2-NEXT:    pcmpeqb %xmm1, %xmm0
; SSE2-NEXT:    pmovmskb %xmm0, %ecx
; SSE2-NEXT:    xorl %eax, %eax
; SSE2-NEXT:    cmpl $65535, %ecx # imm = 0xFFFF
; SSE2-NEXT:    sete %al
; SSE2-NEXT:    retq
;
; AVX2-LABEL: eq_i128:
; AVX2:       # BB#0:
; AVX2-NEXT:    vpcmpeqb %xmm1, %xmm0, %xmm0
; AVX2-NEXT:    vpmovmskb %xmm0, %ecx
; AVX2-NEXT:    xorl %eax, %eax
; AVX2-NEXT:    cmpl $65535, %ecx # imm = 0xFFFF
; AVX2-NEXT:    sete %al
; AVX2-NEXT:    retq
  %bcx = bitcast <2 x i64> %x to i128
  %bcy = bitcast <2 x i64> %y to i128
  %cmp = icmp eq i128 %bcx, %bcy
  %zext = zext i1 %cmp to i32
  ret i32 %zext
}

define i32 @ne_i256(<4 x i64> %x, <4 x i64> %y) {
; SSE2-LABEL: ne_i256:
; SSE2:       # BB#0:
; SSE2-NEXT:    pshufd {{.*#+}} xmm4 = xmm0[2,3,0,1]
; SSE2-NEXT:    movq %xmm4, %rax
; SSE2-NEXT:    pshufd {{.*#+}} xmm4 = xmm1[2,3,0,1]
; SSE2-NEXT:    movq %xmm4, %rcx
; SSE2-NEXT:    movq %xmm0, %rdx
; SSE2-NEXT:    movq %xmm1, %r8
; SSE2-NEXT:    pshufd {{.*#+}} xmm0 = xmm2[2,3,0,1]
; SSE2-NEXT:    movq %xmm0, %rdi
; SSE2-NEXT:    xorq %rax, %rdi
; SSE2-NEXT:    pshufd {{.*#+}} xmm0 = xmm3[2,3,0,1]
; SSE2-NEXT:    movq %xmm0, %rsi
; SSE2-NEXT:    xorq %rcx, %rsi
; SSE2-NEXT:    orq %rdi, %rsi
; SSE2-NEXT:    movq %xmm2, %rax
; SSE2-NEXT:    xorq %rdx, %rax
; SSE2-NEXT:    movq %xmm3, %rcx
; SSE2-NEXT:    xorq %r8, %rcx
; SSE2-NEXT:    orq %rax, %rcx
; SSE2-NEXT:    xorl %eax, %eax
; SSE2-NEXT:    orq %rsi, %rcx
; SSE2-NEXT:    setne %al
; SSE2-NEXT:    retq
;
; AVX2-LABEL: ne_i256:
; AVX2:       # BB#0:
; AVX2-NEXT:    vpcmpeqb %ymm1, %ymm0, %ymm0
; AVX2-NEXT:    vpmovmskb %ymm0, %ecx
; AVX2-NEXT:    xorl %eax, %eax
; AVX2-NEXT:    cmpl $-1, %ecx
; AVX2-NEXT:    setne %al
; AVX2-NEXT:    vzeroupper
; AVX2-NEXT:    retq
  %bcx = bitcast <4 x i64> %x to i256
  %bcy = bitcast <4 x i64> %y to i256
  %cmp = icmp ne i256 %bcx, %bcy
  %zext = zext i1 %cmp to i32
  ret i32 %zext
}

define i32 @eq_i256(<4 x i64> %x, <4 x i64> %y) {
; SSE2-LABEL: eq_i256:
; SSE2:       # BB#0:
; SSE2-NEXT:    pshufd {{.*#+}} xmm4 = xmm0[2,3,0,1]
; SSE2-NEXT:    movq %xmm4, %rax
; SSE2-NEXT:    pshufd {{.*#+}} xmm4 = xmm1[2,3,0,1]
; SSE2-NEXT:    movq %xmm4, %rcx
; SSE2-NEXT:    movq %xmm0, %rdx
; SSE2-NEXT:    movq %xmm1, %r8
; SSE2-NEXT:    pshufd {{.*#+}} xmm0 = xmm2[2,3,0,1]
; SSE2-NEXT:    movq %xmm0, %rdi
; SSE2-NEXT:    xorq %rax, %rdi
; SSE2-NEXT:    pshufd {{.*#+}} xmm0 = xmm3[2,3,0,1]
; SSE2-NEXT:    movq %xmm0, %rsi
; SSE2-NEXT:    xorq %rcx, %rsi
; SSE2-NEXT:    orq %rdi, %rsi
; SSE2-NEXT:    movq %xmm2, %rax
; SSE2-NEXT:    xorq %rdx, %rax
; SSE2-NEXT:    movq %xmm3, %rcx
; SSE2-NEXT:    xorq %r8, %rcx
; SSE2-NEXT:    orq %rax, %rcx
; SSE2-NEXT:    xorl %eax, %eax
; SSE2-NEXT:    orq %rsi, %rcx
; SSE2-NEXT:    sete %al
; SSE2-NEXT:    retq
;
; AVX2-LABEL: eq_i256:
; AVX2:       # BB#0:
; AVX2-NEXT:    vpcmpeqb %ymm1, %ymm0, %ymm0
; AVX2-NEXT:    vpmovmskb %ymm0, %ecx
; AVX2-NEXT:    xorl %eax, %eax
; AVX2-NEXT:    cmpl $-1, %ecx
; AVX2-NEXT:    sete %al
; AVX2-NEXT:    vzeroupper
; AVX2-NEXT:    retq
  %bcx = bitcast <4 x i64> %x to i256
  %bcy = bitcast <4 x i64> %y to i256
  %cmp = icmp eq i256 %bcx, %bcy
  %zext = zext i1 %cmp to i32
  ret i32 %zext
}

