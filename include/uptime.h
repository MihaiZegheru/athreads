#ifndef UPTIME_H__
#define UPTIME_H__

#include <stdint.h>

/**
 * Initialize the uptime counter.
 */
void uptime_init(void);

/**
 * Get the current uptime in milliseconds.
 */
uint32_t uptime_ms(void);

/**
 * Register an event to be called at regular intervals.
 * @param event The function to call when the event is triggered.
 * @param delay_ms The delay in milliseconds between the same events.
 */
void uptime_register_event(void (*event)(void), uint32_t delay_ms);

#endif // UPTIME_H__
