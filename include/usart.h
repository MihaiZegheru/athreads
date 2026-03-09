#ifndef USART_H__
#define USART_H__

#include <avr/io.h>
#include <stdint.h>

/*
 * Initialises serial communication.
 */
void USART0_init(uint16_t  ubrr);

/*
 * Transmits a character through USART.
 */
void USART0_tx(char data);

/*
 * Receives a character through USART
 */
char USART0_rx();

/*
 * Writes a formatted string of chars through USART.
 */
int USART0_printf(const char *fmt, ...); 

#endif // USART_H__
