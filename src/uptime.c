#include "uptime.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#define EVENTS_COUNT 1

volatile uint32_t g_uptime_ms = 0;

static void (*s_events[EVENTS_COUNT])(void);
static uint32_t s_event_delays[EVENTS_COUNT];
static uint8_t s_event_count = 0;

ISR(TIMER2_COMPA_vect) {
    g_uptime_ms++;

    for (uint8_t i = 0; i < s_event_count; i++) {
        if (g_uptime_ms % s_event_delays[i] == 0) {
            s_events[i]();
        }
    }
}

void uptime_init(void) {
    TCCR2A = (1 << WGM21);
    TCCR2B = 0;
    TCNT2  = 0;
    OCR2A  = 249;
    TIMSK2 = (1 << OCIE2A);
    TCCR2B = (1 << CS22);
    sei();
}

uint32_t uptime_ms(void) {
    uint8_t sreg = SREG;
    cli();
    uint32_t t = g_uptime_ms;
    SREG = sreg;
    return t;
}

void uptime_register_event(void (*event)(void), uint32_t delay_ms) {
    if (s_event_count >= EVENTS_COUNT) {
        return;
    }
    s_events[s_event_count] = event;
    s_event_delays[s_event_count] = delay_ms;
    s_event_count++;
}
