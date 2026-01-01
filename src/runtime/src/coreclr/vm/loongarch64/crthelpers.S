// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

#include "unixasmmacros.inc"

// JIT_MemSet/JIT_MemCpy
//
// It is IMPORTANT that the exception handling code is able to find these guys
// on the stack, but on non-windows platforms we can just defer to the platform
// implementation.
//
LEAF_ENTRY JIT_MemSet, _TEXT
    beq $a2, $zero, LOCAL_LABEL(JIT_MemSet_ret)

    ld.b  $zero, $a0, 0     //Is this really needed ?
    //b  memset
    //TODO:#32267 Fix the REG_SECRET_STUB_PARAM(REG_T2) conflict within glibc memset().
    addi.w          $t1, $zero, 7
    move            $t0, $a0
    bgeu            $t1, $a2, 156
    lu12i.w         $t1, 4112
    ori             $t1, $t1, 0x101
    lu32i.d         $t1, 65793
    bstrpick.w      $t3, $a1, 0x7, 0x0
    lu52i.d         $t1, $t1, 16
    andi            $t5, $a0, 0x7
    mul.d           $t1, $t3, $t1
    beqz            $t5, 168
    st.b            $t3, $t0, 0
    addi.d          $t0, $t0, 1
    andi            $t5, $t0, 0x7
    bnez            $t5, -12
    add.d           $a2, $a0, $a2
    sub.d           $a2, $a2, $t0
    srli.d          $t5, $a2, 0x6
    beqz            $t5, 156
    slli.d          $t3, $t5, 0x6
    move            $t4, $t0
    add.d           $t3, $t3, $t0
    stptr.d         $t1, $t0, 0
    st.d            $t1, $t0, 8
    st.d            $t1, $t0, 16
    st.d            $t1, $t0, 24
    st.d            $t1, $t0, 32
    st.d            $t1, $t0, 40
    st.d            $t1, $t0, 48
    st.d            $t1, $t0, 56
    addi.d          $t0, $t0, 64
    bne             $t0, $t3, -36
    slli.d          $t5, $t5, 0x6
    add.d           $t5, $t5, $t4
    bstrpick.d      $t0, $a2, 0x5, 0x3
    beqz            $t0, 56
    alsl.d          $t0, $t0, $t5, 0x3
    stptr.d         $t1, $t5, 0
    addi.d          $t5, $t5, 8
    bne             $t5, $t0, -8
    andi            $a2, $a2, 0x7
    beqz            $a2, 28
    bstrpick.w      $a1, $a1, 0x7, 0x0
    add.d           $a2, $t0, $a2
    st.b            $a1, $t0, 0
    addi.d          $t0, $t0, 1
    bne             $t0, $a2, -8
    jirl            $zero, $ra, 0
    jirl            $zero, $ra, 0
    move            $t0, $t5
    andi            $a2, $a2, 0x7
    b               -40
    srli.d          $t5, $a2, 0x6
    bnez            $t5, -136
    srli.d          $t0, $a2, 0x3
    move            $t5, $a0
    b               -80
    move            $t5, $t0
    b               -96

LOCAL_LABEL(JIT_MemSet_ret):
    jirl  $r0, $ra, 0

////NOTO: Here must use LEAF_END_MARKED!  not LEAF_END !!!
LEAF_END_MARKED JIT_MemSet, _TEXT

LEAF_ENTRY JIT_MemCpy, _TEXT
    beq $a2, $zero, LOCAL_LABEL(JIT_MemCpy_ret)

    ld.b  $zero, $a0, 0
    ld.b  $zero, $a1, 0    //Is this really needed ?

    b  memcpy

LOCAL_LABEL(JIT_MemCpy_ret):
    jirl  $r0, $ra, 0

////NOTO: Here must use LEAF_END_MARKED!  not LEAF_END !!!
LEAF_END_MARKED JIT_MemCpy, _TEXT
