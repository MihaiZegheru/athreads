#include "usart.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define IO_BUFF_SIZE 64

char USART0_pbuff[IO_BUFF_SIZE];
char USART0_sbuff[IO_BUFF_SIZE];

void USART0_init(uint16_t ubrr) {
    UBRR0H = (uint8_t)(ubrr>>8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1<<RXEN0) | (1<<TXEN0);
    UCSR0C = (3<<UCSZ00);
}

void USART0_tx(char data) {
    while(!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

char USART0_rx() {
    while(!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

int USART0_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(USART0_pbuff, sizeof(USART0_pbuff), fmt, args);

    va_end(args);

    char *src = USART0_pbuff;
    while (*src) {
        USART0_tx(*src++);
    }
    
    return len;
}
