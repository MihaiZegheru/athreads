#include <avr/io.h>

#include "usart.h"

#define CLOCK_SPEED 16000000UL
#define BAUD 9600
#define MYUBRR (CLOCK_SPEED / 16 / BAUD - 1)

int main(void) {
    USART0_init(MYUBRR);

    int num = 2560;
    USART0_printf("Hello World from ATmega%d\r\n", num);

    while(1) {}

    return 0;
}
