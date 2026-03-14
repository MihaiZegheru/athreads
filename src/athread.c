#include "athread.h"

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "debug.h"

void athread_start_asm(void);

#define MAX_THREADS 8
#define STACK_SIZE  512
#define IDLE_TID    0

volatile athread_t threads[MAX_THREADS];
volatile uint8_t   thread_count;
volatile uint8_t   current_tid;

static uint8_t stacks[MAX_THREADS][STACK_SIZE];
static void *athread_info[MAX_THREADS];

static void athread_idle(void *info);

static void athread_create_internal(uint8_t tid, athread_entry_t entry) {
	uint8_t sreg = SREG;
	cli();

    threads[tid] = (athread_t){
        .entry = entry,
        .sp = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .stack_low = (uint16_t)&stacks[tid][0],
        .stack_high = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .tid = tid,
        .state = TS_READY
    };

	SREG = sreg;
}

void athread_init(void) {
	thread_count = 1;
	current_tid = IDLE_TID;

	for (uint8_t i = 0; i < MAX_THREADS; i++) {
        threads[i] = (athread_t){
            .entry = 0,
            .sp = 0,
            .stack_low = (uint16_t)&stacks[i][0],
            .stack_high = (uint16_t)&stacks[i][STACK_SIZE - 1],
            .tid = i,
            .state = TS_UNUSED
        };
	}

	athread_create_internal(IDLE_TID, athread_idle);
}

void athread_start(void) {
	sei();

	uint8_t selected = IDLE_TID;
	for (uint8_t i = 0; i < MAX_THREADS; i++) {
		if (i != IDLE_TID && threads[i].state == TS_READY) {
			selected = i;
			break;
		}
	}
	current_tid = selected;
	threads[selected].state = TS_RUNNING;

	LOG_DEBUG("Starting scheduler with thread %u", selected);

	athread_start_asm();
}

uint8_t athread_create(athread_entry_t entry, void *info) {
	if (entry == 0 || thread_count >= MAX_THREADS) {
		return ATHREAD_INVALID_TID;
	}

    uint8_t tid = thread_count++;
    athread_info[tid] = info;
    athread_create_internal(tid, entry);

	LOG_DEBUG("Created thread %u", tid);

	return tid;
}

static void athread_idle(void *info) {
	for (;;) {
		athread_yield();
	}
}

void athread_bootstrap(void) {
	if (threads[current_tid].entry != 0) {
		threads[current_tid].entry(athread_info[current_tid]);
	}

	threads[current_tid].state = TS_ZOMBIE;

    // TODO: Handle joining logic.

	for (;;) {
		athread_yield();
	}
}

