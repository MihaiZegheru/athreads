#ifndef ENCODER_H__
#define ENCODER_H__

#include <stdint.h>

/**
 * Thread entry for the rotary encoder process.
 *
 * The encoder pins are initialized here, while edge sampling is performed by
 * encoder_poll_tick() from the timer interrupt.
 *
 * @param info Unused thread argument.
 */
void encoder_thread(void *info) __attribute__((no_instrument_function));

/**
 * Gets the accumulated rotary encoder position.
 *
 * Positive values represent clockwise movement and negative values represent
 * counter-clockwise movement, relative to startup.
 *
 * @return The current encoder position.
 */
int16_t encoder_get_value(void) __attribute__((no_instrument_function));

/**
 * Gets the number of debounced button presses detected on the encoder switch.
 *
 * The value is cumulative and wraps at 255.
 *
 * @return The button press counter.
 */
uint8_t encoder_get_button_presses(void) __attribute__((no_instrument_function));

/**
 * Polls and debounces the rotary encoder pins.
 *
 * This function is intended to run from the 1 ms uptime timer interrupt so the
 * menu can read stable movement and button events from thread context.
 */
void encoder_poll_tick(void) __attribute__((no_instrument_function));

#endif // ENCODER_H__
