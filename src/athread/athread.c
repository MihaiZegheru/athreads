#include "athread/athread.h"

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "platform/debug.h"

#define MAX_THREADS ATHREAD_MAX_THREADS
#define IDLE_TID    0

#define IDLE_STACK_SIZE    192

volatile athread_t threads[MAX_THREADS];
volatile uint8_t   thread_count;
volatile uint8_t   current_tid;

static uint8_t stack_pool[ATHREAD_STACK_POOL_SIZE];
static uint16_t stack_pool_used;
static void *athread_info[MAX_THREADS];
static volatile uint8_t thread_quantum_ticks[MAX_THREADS];
static volatile uint8_t thread_remaining_ticks[MAX_THREADS];
static volatile uint32_t thread_cpu_ticks[MAX_THREADS];

static void athread_idle(void *info);
static void athread_timer_init(void);
uint8_t athread_quantum_should_switch(void) __attribute__((no_instrument_function));
void athread_on_thread_selected(uint8_t tid) __attribute__((no_instrument_function));

void athread_start_asm(void);
void athread_isr(void);

__attribute__((no_instrument_function))
static uint16_t athread_align_stack_size(uint16_t stack_size) {
	if (stack_size < ATHREAD_MIN_STACK_SIZE) {
		stack_size = ATHREAD_MIN_STACK_SIZE;
	}
	return (uint16_t)((stack_size + 1u) & (uint16_t)~1u);
}

__attribute__((no_instrument_function))
static uint8_t *athread_alloc_stack(uint16_t stack_size) {
	stack_size = athread_align_stack_size(stack_size);

	if ((uint32_t)stack_pool_used + stack_size > ATHREAD_STACK_POOL_SIZE) {
		return 0;
	}

	uint8_t *stack = &stack_pool[stack_pool_used];
	stack_pool_used = (uint16_t)(stack_pool_used + stack_size);
	return stack;
}

__attribute__((no_instrument_function))
uint8_t athread_get_current_tid(void) {
	uint8_t sreg = SREG;
	cli();
	uint8_t tid = current_tid;
	SREG = sreg;
	return tid;
}

static uint8_t athread_create_internal(uint8_t tid, athread_entry_t entry, uint16_t stack_size) {
	stack_size = athread_align_stack_size(stack_size);
	uint8_t *stack = athread_alloc_stack(stack_size);
	if (stack == 0) {
		return 0;
	}
	
    threads[tid] = (athread_t){
        .entry = entry,
        .sp = (uint16_t)&stack[stack_size - 1],
        .stack_low = (uint16_t)&stack[0],
        .stack_high = (uint16_t)&stack[stack_size - 1],
        .tid = tid,
        .state = TS_READY,
		.wake_ticks = 0
    };
	thread_quantum_ticks[tid] = ATHREAD_MIN_QUANTUM_TICKS;
	thread_remaining_ticks[tid] = ATHREAD_MIN_QUANTUM_TICKS;
	return 1;
}

void athread_init(void) {
	thread_count = 1;
	current_tid = IDLE_TID;
	stack_pool_used = 0;

	for (uint8_t i = 0; i < MAX_THREADS; i++) {
        threads[i] = (athread_t){
            .entry = 0,
            .sp = 0,
            .stack_low = 0,
            .stack_high = 0,
            .tid = i,
            .state = TS_UNUSED,
            .wake_ticks = 0
        };
		athread_info[i] = 0;
		thread_quantum_ticks[i] = ATHREAD_MIN_QUANTUM_TICKS;
		thread_remaining_ticks[i] = ATHREAD_MIN_QUANTUM_TICKS;
		thread_cpu_ticks[i] = 0;
	}

	(void)athread_create_internal(IDLE_TID, athread_idle, IDLE_STACK_SIZE);
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
	athread_on_thread_selected(selected);

	LOG_DEBUG("Starting scheduler with thread %u", selected);

	LOG_DEBUG("Starting athread scheduler");
	athread_timer_init();
	LOG_DEBUG("Initialized timer for thread scheduling");

	athread_start_asm();
}

uint8_t athread_create(athread_entry_t entry, void *info, uint16_t stack_size) {
	uint8_t sreg = SREG;
	cli();

	if (entry == 0 || stack_size == 0 || thread_count >= MAX_THREADS) {
		SREG = sreg;
		return ATHREAD_INVALID_TID;
	}

    uint8_t tid = thread_count;
	if (!athread_create_internal(tid, entry, stack_size)) {
		SREG = sreg;
		return ATHREAD_INVALID_TID;
	}

	thread_count++;
    athread_info[tid] = info;

	SREG = sreg;
	LOG_DEBUG("Created thread %u", tid);

	return tid;
}

__attribute__((no_instrument_function))
uint8_t athread_set_quantum(uint8_t tid, uint8_t quantum_ticks) {
	uint8_t sreg = SREG;
	cli();

	if (tid >= thread_count || threads[tid].state == TS_UNUSED) {
		SREG = sreg;
		return 0;
	}

	if (quantum_ticks < ATHREAD_MIN_QUANTUM_TICKS) {
		quantum_ticks = ATHREAD_MIN_QUANTUM_TICKS;
	}

	thread_quantum_ticks[tid] = quantum_ticks;
	if (thread_remaining_ticks[tid] > quantum_ticks) {
		thread_remaining_ticks[tid] = quantum_ticks;
	}

	SREG = sreg;
	return 1;
}

__attribute__((no_instrument_function))
uint8_t athread_get_quantum(uint8_t tid) {
	uint8_t sreg = SREG;
	cli();

	if (tid >= thread_count || threads[tid].state == TS_UNUSED) {
		SREG = sreg;
		return 0;
	}

	uint8_t quantum_ticks = thread_quantum_ticks[tid];
	SREG = sreg;
	return quantum_ticks;
}

__attribute__((no_instrument_function))
uint8_t athread_get_thread_count(void) {
	uint8_t sreg = SREG;
	cli();
	uint8_t count = thread_count;
	SREG = sreg;
	return count;
}

__attribute__((no_instrument_function))
uint8_t athread_get_cpu_ticks(uint32_t *out_ticks, uint8_t max_ticks) {
	uint8_t sreg = SREG;
	cli();

	uint8_t count = thread_count;
	if (count > max_ticks) {
		count = max_ticks;
	}

	for (uint8_t i = 0; i < count; i++) {
		out_ticks[i] = thread_cpu_ticks[i];
	}

	SREG = sreg;
	return count;
}

__attribute__((no_instrument_function))
uint8_t athread_quantum_should_switch(void) {
	uint8_t tid = current_tid;

	if (tid >= thread_count || threads[tid].state == TS_UNUSED) {
		return 1;
	}

	thread_cpu_ticks[tid]++;

	if (thread_remaining_ticks[tid] > 1) {
		thread_remaining_ticks[tid]--;
		return 0;
	}

	thread_remaining_ticks[tid] = thread_quantum_ticks[tid];
	return 1;
}

__attribute__((no_instrument_function))
void athread_on_thread_selected(uint8_t tid) {
	if (tid < thread_count && threads[tid].state != TS_UNUSED) {
		thread_remaining_ticks[tid] = thread_quantum_ticks[tid];
	}
}

void athread_sleep_ticks(uint8_t ticks) {
	uint8_t sreg = SREG;
	cli();

	if (ticks == 0) {
		SREG = sreg;
		athread_yield();
		return;
	}

	threads[current_tid].wake_ticks = ticks;
	threads[current_tid].state = TS_SLEEPING;

	SREG = sreg;
	athread_yield();
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

	for (;;) {
		athread_yield();
	}
}

void athread_tick(void) {
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
	OCR1A = 1249;
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect, __attribute__((no_instrument_function))) {
	athread_isr();
}
