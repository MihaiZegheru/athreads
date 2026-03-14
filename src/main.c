#include "usart.h"
#include "uptime.h"
#include "athread.h"
#include "debug.h"

#define CLOCK_SPEED 16000000UL
#define BAUD 9600
#define MYUBRR (CLOCK_SPEED / 16 / BAUD - 1)

volatile static uint8_t s_flag = 0;
uint8_t tids[8] = {1, 2, 3, 4, 5, 6, 7, 8};

void timer_event() {
    s_flag = 1;
} 

/**
 * Placeholder entry for create test.
 * */
static void dummy_thread(void *info) {
    int id = *(uint8_t*)info;
    for (int i = 0; i < 10000; i++) {
        if (i % 1000 == 0) {
            LOG_INFO("Thread %d running: %u", id, i);
        }
        athread_yield();
    }
}

/**
 * Test creating more threads than the maximum allowed and check correct handling of invalid TIDs.
 * The first 7 calls to athread_create should succeed, while the 8th call should fail and return
 * ATHREAD_INVALID_TID.
 * Logs the results of each thread creation attempt for debugging purposes.
 */
static void test_athread_create() {
    for (uint8_t i = 0; i < 9; i++) {
        uint8_t tid = athread_create(dummy_thread, tids + i);
        if (tid == ATHREAD_INVALID_TID && i < 6) {
            LOG_ERROR("athread_create failed at iteration %u", i);
        } else {
            LOG_INFO("athread_create ok: %u", tid);
        }
    }
}

static void main_thread(void *info) {
    LOG_INFO("Main thread running");
    
    uptime_register_event(timer_event, 1000);

    test_athread_create();

    while(1) {
        if (s_flag) {
            s_flag = 0;
            LOG_INFO("Uptime: %lu ms", uptime_ms());
        }
        athread_yield();
    }
}

int main(void) {
    USART0_init(MYUBRR);
    uptime_init();

    athread_init();
    athread_create(main_thread, 0);
    athread_start(); 

    return 0;
}
