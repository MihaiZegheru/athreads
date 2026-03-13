#include "athread.h"

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define MAX_THREADS 8
#define STACK_SIZE  256
#define IDLE_TID 0

volatile athread_t threads[MAX_THREADS];
volatile uint8_t  thread_count;
volatile uint8_t  current_tid;

static uint8_t stacks[MAX_THREADS][STACK_SIZE];


uint8_t athread_create(athread_entry_t entry) {
	if (entry == 0 || thread_count >= MAX_THREADS) {
		return ATHREAD_INVALID_TID;
	}

	uint8_t sreg = SREG;
	cli();

    uint8_t tid = ++thread_count;

    threads[tid] = (athread_t){
        .entry = entry,
        .sp = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .stack_low = (uint16_t)&stacks[tid][0],
        .stack_high = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .tid = tid,
        .state = TS_READY
    };
	SREG = sreg;

	return tid;
}
