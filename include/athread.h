#ifndef ATHREAD_H__
#define ATHREAD_H__

#include <stdalign.h>
#include <stdint.h>

#define ATHREAD_INVALID_TID 0xFF

typedef void (*athread_entry_t)(void);

/**
 * Thread states for the athread library.
 * - TS_UNUSED: The thread slot is unused and can be allocated.
 * - TS_READY: The thread is ready to run but not currently running.
 * - TS_RUNNING: The thread is currently running.
 * - TS_BLOCKED: The thread is blocked, waiting for an event or resource.
 * - TS_SLEEPING: The thread is sleeping for a specified duration.
 * - TS_ZOMBIE: The thread has finished execution but has not been cleaned up yet.
 */
typedef enum uint8_t {
    TS_UNUSED = 0,
    TS_READY,
    TS_RUNNING,
    TS_BLOCKED,
    TS_SLEEPING,
    TS_ZOMBIE
} athread_state_t;

/**
 * Structure representing a thread in the athread library. It contains:
 * - thread ID
 * - state
 * - entry function
 * - stack pointer
 * - stack boundaries
 */
typedef struct __attribute__((aligned(2))) {
    athread_entry_t entry;

    uint16_t sp;
    uint16_t stack_low;
    uint16_t stack_high;
    
    uint8_t tid;
    athread_state_t state;
} athread_t;

/**
 * Creates a new thread with the given entry function. If the entry function is null or the
 * maximum number of threads has been reached, the function will return ATHREAD_INVALID_TID
 * without creating a thread. Otherwise, it will return the thread ID.
 * 
 * @param entry The function that the thread will execute when it runs.
 * @return The thread ID if successful, or ATHREAD_INVALID_TID if failed.
 */
uint8_t athread_create(athread_entry_t entry);

#endif // ATHREAD_H__
