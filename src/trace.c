#include "trace.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "athread.h"
#include "uptime.h"

#define TRACE_SIZE 400

volatile trace_event_t trace_buf[TRACE_SIZE];
volatile uint16_t trace_head = 0;
volatile uint16_t trace_tail = 0;

__attribute__((no_instrument_function))
static void trace_push(trace_type_t type, uint8_t tid, uintptr_t fn) {
    uint16_t next = (uint16_t)((trace_head + 1) % TRACE_SIZE);

    // drop new event if buffer is full
    if (next == trace_tail) {
        return;
    }

    trace_buf[trace_head] = (trace_event_t){
        .timestamp = uptime_ms(),
        .fn = fn,
        .type = type,
        .tid = tid
    };

    trace_head = next;
}

uint8_t trace_read_batch(trace_event_t *buf, uint8_t max_events) {
    uint8_t sreg = SREG;
    cli();

    uint8_t count = 0;

    while (count < max_events && trace_tail != trace_head) {
        buf[count++] = trace_buf[trace_tail];
        trace_tail = (uint16_t)((trace_tail + 1) % TRACE_SIZE);
    }

    SREG = sreg;
    return count;
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *this_fn, void *call_site) {
    uint8_t sreg = SREG;
    cli();
    trace_push(TT_TRACE_ENTER, athread_get_current_tid(), (uintptr_t)this_fn);
    SREG = sreg;
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *this_fn, void *call_site) {
    uint8_t sreg = SREG;
    cli();
    trace_push(TT_TRACE_EXIT, athread_get_current_tid(), (uintptr_t)this_fn);
    SREG = sreg;
}
