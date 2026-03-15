#ifndef USART_H__
#define USART_H__

#include <avr/io.h>
#include <stdint.h>

/*
 * Initialises serial communication.
 */
void usart_init(uint16_t  ubrr);

/*
 * Transmits a character through USART.
 */
void usart_tx(char data);

/*
 * Receives a character through USART
 */
char usart_rx();

/*
 * Writes a formatted string of chars through USART.
 */
int usart_printf(const char *fmt, ...);

/*
 * Sends an array of bytes through USART.
 */
void usart_send_bytes(uint8_t *data, uint8_t len);

#endif // USART_H__
