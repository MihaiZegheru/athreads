#include "usart.h"
#include "uptime.h"
#include "athread.h"

#define CLOCK_SPEED 16000000UL
#define BAUD 9600
#define MYUBRR (CLOCK_SPEED / 16 / BAUD - 1)

volatile static uint8_t s_flag = 0;

void timer_event() {
    s_flag = 1;
} 

/**
 * Placeholder entry for create test.
 * */
static void dummy_thread(void) { }

void Test_athread_create() {
    for (uint8_t i = 0; i < 9; i++) {
        uint8_t tid = athread_create(dummy_thread);
        if (tid == ATHREAD_INVALID_TID && i < 8) {
            USART0_printf("athread_create failed at iteration %u\r\n", i);
        } else {
            USART0_printf("athread_create ok: %u\r\n", tid);
        }
    }
}

int main(void) {
    USART0_init(MYUBRR);

    int num = 2560;
    USART0_printf("Hello World from ATmega%d\r\n", num);

    uptime_init();
    uptime_register_event(timer_event, 1000);

    Test_athread_create();

    while(1) {
        if (s_flag) {
            s_flag = 0;
            USART0_printf("Uptime: %lu ms\r\n", uptime_ms());
        }
    }

    return 0;
}
