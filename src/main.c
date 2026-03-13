#include "usart.h"
#include "uptime.h"

#define CLOCK_SPEED 16000000UL
#define BAUD 9600
#define MYUBRR (CLOCK_SPEED / 16 / BAUD - 1)

volatile static uint8_t s_flag = 0;

void timer_event() {
    s_flag = 1;
} 

int main(void) {
    USART0_init(MYUBRR);

    int num = 2560;
    USART0_printf("Hello World from ATmega%d\r\n", num);

    uptime_init();
    uptime_register_event(timer_event, 1000);

    while(1) {
        if (s_flag) {
            s_flag = 0;
            USART0_printf("Uptime: %lu ms\r\n", uptime_ms());
        }
    }

    return 0;
}
