#include "athread.h"

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "debug.h"

#define MAX_THREADS 8
#define STACK_SIZE  512
#define IDLE_TID    0

volatile athread_t threads[MAX_THREADS];
volatile uint8_t   thread_count;
volatile uint8_t   current_tid;

static uint8_t stacks[MAX_THREADS][STACK_SIZE];
static void *athread_info[MAX_THREADS];

static void athread_idle(void *info);
static void athread_timer_init(void);

void athread_start_asm(void);
void athread_isr(void);

static void athread_create_internal(uint8_t tid, athread_entry_t entry) {
	uint8_t sreg = SREG;
	cli();

    threads[tid] = (athread_t){
        .entry = entry,
        .sp = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .stack_low = (uint16_t)&stacks[tid][0],
        .stack_high = (uint16_t)&stacks[tid][STACK_SIZE - 1],
        .tid = tid,
        .state = TS_READY,
		.wake_ticks = 0
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
            .state = TS_UNUSED,
            .wake_ticks = 0
        };
	}

	athread_create_internal(IDLE_TID, athread_idle);
}

/**
 * Initializes the timer and starts the scheduler.
 */
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

	LOG_DEBUG("Starting athread scheduler");
	athread_timer_init();
	LOG_DEBUG("Initialized timer for thread scheduling");

	athread_start_asm();
}

uint8_t athread_create(athread_entry_t entry, void *info) {
	uint8_t sreg = SREG;
	cli();

	if (entry == 0 || thread_count >= MAX_THREADS) {
		SREG = sreg;
		return ATHREAD_INVALID_TID;
	}

    uint8_t tid = thread_count++;
    athread_info[tid] = info;
    athread_create_internal(tid, entry);

	SREG = sreg;
	LOG_DEBUG("Created thread %u", tid);

	return tid;
}

static void athread_idle(void *info) {
	for (;;) {
		athread_yield();
	}
}

void athread_bootstrap(void) {
	sei();
	if (threads[current_tid].entry != 0) {
		threads[current_tid].entry(athread_info[current_tid]);
	}

	threads[current_tid].state = TS_ZOMBIE;

    // TODO: Handle joining logic.

	for (;;) {
		athread_yield();
	}
}

void athread_tick(void) {

	// TODO: Add custom waiting amount for each thread. For now this will only wake up sleeping
	// threads that have voluntarily called sleep (not implemented yet).

	for (uint8_t i = 0; i < thread_count; i++) {
		if (threads[i].state == TS_SLEEPING) {
			if (threads[i].wake_ticks > 0) {
				threads[i].wake_ticks--;
			}
			if (threads[i].wake_ticks == 0) {
				threads[i].state = TS_READY;
			}
		}
	}
}

static void athread_timer_init(void) {
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	OCR1A = 12499;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
	athread_isr();
}
