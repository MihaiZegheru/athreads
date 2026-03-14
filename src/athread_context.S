#include <avr/io.h>

#include "athread_offsets.h"

    .global athread_yield
    .global athread_start_asm

    .extern athread_size
    .extern current_tid
    .extern thread_count
    .extern threads
    .extern athread_bootstrap
.text

athread_start_asm:
    lds  r18, current_tid
    ldi  r19, 0

; mark threads[current_tid].started = 1
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22          ; r1:r0 = tid * size
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STARTED_OFFSET
    add  r26, r30
    adc  r27, r1
    ldi  r20, 1
    st   X, r20

; load threads[current_tid].stack_high into SP
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STACK_HIGH_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r28, X+            ; stack_high low
    ld   r29, X             ; stack_high high

    out  _SFR_IO_ADDR(SPL), r28
    out  _SFR_IO_ADDR(SPH), r29

    ldi  r24, 0x00
#if defined(RAMPX)
    out  _SFR_IO_ADDR(RAMPX), r24
#endif
#if defined(RAMPY)
    out  _SFR_IO_ADDR(RAMPY), r24
#endif
    out  _SFR_IO_ADDR(RAMPZ), r24
#if defined(EIND)
    out  _SFR_IO_ADDR(EIND), r24
#endif

    clr  r1
    jmp  athread_bootstrap

athread_yield:
    push r0
    in   r0, _SFR_IO_ADDR(SREG)
    cli
    push r0
    push r1
    clr  r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push r16
    push r17
    push r18
    push r19
    push r20
    push r21
    push r22
    push r23
    push r24
    push r25
    push r26
    push r27
    push r28
    push r29
    push r30
    push r31

; Save the extended registers if they exist
#if defined(RAMPX)
    in   r0, _SFR_IO_ADDR(RAMPX)
    push r0
#endif
#if defined(RAMPY)
    in   r0, _SFR_IO_ADDR(RAMPY)
    push r0
#endif
#if defined(RAMPZ)
    in   r0, _SFR_IO_ADDR(RAMPZ)
    push r0
#endif
#if defined(EIND)
    in   r0, _SFR_IO_ADDR(EIND)
    push r0
#endif

; Save the stack pointer
    in   r24, _SFR_IO_ADDR(SPL)
    in   r25, _SFR_IO_ADDR(SPH)

    lds  r30, current_tid
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22       ; r1:r0 = tid * size
    mov  r30, r0
    mov  r31, r1
    clr  r1

    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_SP_OFFSET
    add  r26, r30
    adc  r27, r1

    st   X+, r24
    st   X,  r25

; mark current thread ready if it was running
    lds  r18, current_tid
    ldi  r22, ATHREAD_SIZE
    mul  r18, r22       ; r1:r0 = tid * size
    mov  r30, r0
    mov  r31, r1
    clr  r1

    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STATE_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r20, X
    cpi  r20, TS_RUNNING
    brne state_done
    ldi  r20, TS_READY
    st   X, r20
state_done:

    lds  r18, current_tid

find_next_thread:
    inc  r18
    lds  r19, thread_count
    cp   r18, r19
    brlo candidate_in_range
    ldi  r18, 0

candidate_in_range:
; X = &threads[tid]
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22          ; r1:r0 = tid * size
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

; check threads[tid].state == TS_READY
    ldi  r30, ATHREAD_STATE_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r20, X
    cpi  r20, TS_READY
    brne find_next_thread

    sts  current_tid, r18

; set selected thread state to running
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STATE_OFFSET
    add  r26, r30
    adc  r27, r1
    ldi  r20, TS_RUNNING
    st   X, r20

; check started flag
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STARTED_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r20, X
    tst  r20
    brne restore_existing

    ldi  r20, 1
    st   X, r20

; first start: load stack_high into SP
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

    ldi  r30, ATHREAD_STACK_HIGH_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r24, X+            ; stack_high low
    ld   r25, X             ; stack_high high

    out  _SFR_IO_ADDR(SPL), r24
    out  _SFR_IO_ADDR(SPH), r25

    clr  r1
    jmp  athread_bootstrap

restore_existing:
; X = &threads[tid]
    mov  r30, r18
    ldi  r31, 0
    ldi  r22, ATHREAD_SIZE
    mul  r30, r22          ; r1:r0 = tid * size
    mov  r30, r0
    mov  r31, r1
    clr  r1
    ldi  r26, lo8(threads)
    ldi  r27, hi8(threads)
    add  r26, r30
    adc  r27, r31

; load threads[tid].sp into SPL/SPH
    ldi  r30, ATHREAD_SP_OFFSET
    add  r26, r30
    adc  r27, r1
    ld   r24, X+
    ld   r25, X

    out  _SFR_IO_ADDR(SPL), r24
    out  _SFR_IO_ADDR(SPH), r25

#if defined(EIND)
    pop  r0
    out  _SFR_IO_ADDR(EIND), r0
#endif
#if defined(RAMPZ) 
    pop  r0
    out  _SFR_IO_ADDR(RAMPZ), r0
#endif
#if defined(RAMPY)
    pop  r0
    out  _SFR_IO_ADDR(RAMPY), r0
#endif
#if defined(RAMPX)
    pop  r0
    out  _SFR_IO_ADDR(RAMPX), r0
#endif

    pop  r31
    pop  r30
    pop  r29
    pop  r28
    pop  r27
    pop  r26
    pop  r25
    pop  r24
    pop  r23
    pop  r22
    pop  r21
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  r7
    pop  r6
    pop  r5
    pop  r4
    pop  r3
    pop  r2
    pop  r1
    pop  r0
    out  _SFR_IO_ADDR(SREG), r0
    pop  r0
    ret