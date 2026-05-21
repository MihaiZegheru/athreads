#include "platform/usart.h"
#include "platform/uptime.h"
#include "athread/athread.h"
#include "platform/debug.h"
#include "ui/spi_screen.h"
#include "ui/encoder.h"
#include "profiling/cpu_stats.h"
#include "profiling/worker_demo.h"

#define CLOCK_SPEED 16000000UL
#define BAUD 115200
#define MYUBRR ((CLOCK_SPEED / 8 / BAUD) - 1)
#define MAIN_THREAD_STACK_SIZE    320
#define ENCODER_THREAD_STACK_SIZE 192
#define SCREEN_THREAD_STACK_SIZE  512

static volatile uint8_t s_flag = 0;

__attribute__((no_instrument_function))
void timer_event() {
    s_flag = 1;
}

static void main_thread(void *info) {
    (void)info;

    LOG_INFO("Main thread running");
    
    uptime_register_event(timer_event, 250);
    
    uint8_t encoder_thread_tid = athread_create(encoder_thread, 0, ENCODER_THREAD_STACK_SIZE);
    uint8_t screen_thread_tid = athread_create(spi_screen_thread, 0, SCREEN_THREAD_STACK_SIZE);
    worker_demo_create_threads();

    (void)encoder_thread_tid;
    (void)screen_thread_tid;

    while(1) {
        if (s_flag) {
            s_flag = 0;
            LOG_INFO("Uptime: %lu ms", uptime_ms());
            cpu_stats_send();
        }
        athread_yield();

    }
}

__attribute__((no_instrument_function))
int main(void) {
    usart_init(MYUBRR);
    uptime_init();

    athread_init();
    uint8_t main_thread_tid = athread_create(main_thread, 0, MAIN_THREAD_STACK_SIZE);
    (void)main_thread_tid;
    LOG_INFO("Main thread created with TID %u\n", main_thread_tid);
    athread_start(); 

    return 0;
}
