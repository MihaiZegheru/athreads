#ifndef TRACE_H__
#define TRACE_H__

#include <stdint.h>

/**
 * Represents the type of a trace event.
 */
typedef uint8_t trace_type_t;
enum {
    TT_TRACE_ENTER = 1,
    TT_TRACE_EXIT = 2
};

/**
 * Represents a single trace event, which can be either a function entry or exit.
 * The structure is packed to minimize memory usage.
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uintptr_t fn;
    uint8_t type;
    uint8_t tid;
} trace_event_t;

/**
 * Reads up to max_events trace events from the trace buffer into the provided buffer.
 * 
 * @param buf The buffer to store the trace events. Must be at least max_events in size.
 * @param max_events The maximum number of events to read.
 * @return The number of events actually read.
 * @note This function disables interrupts to ensure consistency.
 */
uint8_t trace_read_batch(trace_event_t *buf, uint8_t max_events);

#endif // TRACE_H__
