#include "usart.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/interrupt.h>

#define IO_BUFF_SIZE 64

char usart_pbuff[IO_BUFF_SIZE];

void usart_init(uint16_t ubrr) {
    UCSR0A = (1 << U2X0);
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;

    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

__attribute__((no_instrument_function))
void usart_tx(char data) {
    while(!(UCSR0A & (1<<UDRE0)));
    UDR0 = data;
}

__attribute__((no_instrument_function))
char usart_rx() {
    while(!(UCSR0A & (1<<RXC0)));
    return UDR0;
}

__attribute__((no_instrument_function))
int usart_printf(const char *fmt, ...) {
    uint8_t sreg = SREG;
    cli();

    va_list args;
    va_start(args, fmt);

    int len = vsnprintf(usart_pbuff, sizeof(usart_pbuff), fmt, args);

    va_end(args);

    char *src = usart_pbuff;
    while (*src) {
        usart_tx(*src++);
    }

    SREG = sreg;

    return len;
}

__attribute__((no_instrument_function))
void usart_send_bytes(uint8_t *data, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        while (!(UCSR0A & (1 << UDRE0))) {
        }
        UDR0 = data[i];
    }
}
