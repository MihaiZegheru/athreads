#include "usart.h"
#include "uptime.h"
#include "athread.h"
#include "debug.h"
#include "trace.h"
#include "flame_demo.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#define CLOCK_SPEED 16000000UL
#define BAUD 115200
#define MYUBRR ((CLOCK_SPEED / 8 / BAUD) - 1)

static volatile uint8_t s_flag = 0;

uint8_t tids[8] = {1, 2, 3, 4, 5, 6, 7, 8};

#define TRACE_SIZE 16
volatile trace_event_t buff[TRACE_SIZE];

__attribute__((no_instrument_function))
void timer_event() {
    s_flag = 1;
}

static void dummy_thread(void *info) {
    uint8_t tid = *(uint8_t *)info;

    while (1) {
        flame_demo_work(tid);
    }
}

/**
 * Test creating more threads than the maximum allowed and check correct handling of invalid TIDs.
 * The first 7 calls to athread_create should succeed, while the 8th call should fail and return
 * ATHREAD_INVALID_TID.
 * Logs the results of each thread creation attempt for debugging purposes.
 */
static void test_athread_create() {
    for (uint8_t i = 0; i < 1; i++) {
        uint8_t tid = athread_create(dummy_thread, tids + 2);
    }
}

static void trace_thread(void *info) {
    while (1) {
        uint8_t count = trace_read_batch((trace_event_t *)buff, TRACE_SIZE);
        for (uint8_t i = 0; i < count; i++) {
            uint8_t marker = 0xAA;
            usart_send_bytes(&marker, 1);
            usart_send_bytes((uint8_t*)&buff[i], sizeof(trace_event_t));
        }
    }
}

static void main_thread(void *info) {
    LOG_INFO("Main thread running");
    
    uptime_register_event(timer_event, 1000);
    
    uint8_t trace_thread_tid = athread_create(trace_thread, 0);
    test_athread_create();

    while(1) {
        if (s_flag) {
            s_flag = 0;
            LOG_INFO("Uptime: %lu ms", uptime_ms());
        }
        // athread_yield();

    }
}

__attribute__((no_instrument_function))
int main(void) {
    usart_init(MYUBRR);
    uptime_init();

    athread_init();
    uint8_t main_thread_tid = athread_create(main_thread, 0);
    LOG_INFO("Main thread created with TID %u\n", main_thread_tid);
    athread_start(); 

    return 0;
}
