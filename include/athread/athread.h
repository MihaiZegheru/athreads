#ifndef ATHREAD_H__
#define ATHREAD_H__

#include <stdalign.h>
#include <stdint.h>

#define ATHREAD_INVALID_TID 0xFF
#define ATHREAD_MIN_QUANTUM_TICKS 1
#define ATHREAD_MAX_THREADS 8
#define ATHREAD_MIN_STACK_SIZE 64

#ifndef ATHREAD_STACK_POOL_SIZE
#define ATHREAD_STACK_POOL_SIZE 3904
#endif

typedef void (*athread_entry_t)(void*);

/**
 * Thread states for the athread library.
 * - TS_UNUSED: The thread slot is unused and can be allocated.
 * - TS_READY: The thread is ready to run but not currently running.
 * - TS_RUNNING: The thread is currently running.
 * - TS_BLOCKED: The thread is blocked, waiting for an event or resource.
 * - TS_SLEEPING: The thread is sleeping for a specified duration.
 * - TS_ZOMBIE: The thread has finished execution but has not been cleaned up yet.
 */
typedef uint8_t athread_state_t;
enum {
    TS_UNUSED = 0,
    TS_READY,
    TS_RUNNING,
    TS_BLOCKED,
    TS_SLEEPING,
    TS_ZOMBIE
};

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

    uint8_t started;
    uint8_t wake_ticks;
} athread_t;

/**
 * Initializes the athread library. This function must be called before any other athread functions.
 */
void athread_init(void);

/**
 * Starts the thread scheduler. This function will never return and will switch to the first ready thread.
 * The idle thread will run if no other threads are ready.
 */
void athread_start(void);

/**
 * Creates a new thread with the given entry function and stack size. The stack
 * is allocated from the athread stack pool at runtime. If the entry function is
 * null, the stack size is zero, the maximum number of threads has been reached,
 * or the stack pool does not have enough free space, the function returns
 * ATHREAD_INVALID_TID.
 * 
 * @param entry The function that the thread will execute when it runs.
 * @param info A pointer to the information that will be passed to the thread's entry function.
 * @param stack_size Stack size in bytes. Values below ATHREAD_MIN_STACK_SIZE are clamped.
 * @return The thread ID if successful, or ATHREAD_INVALID_TID if failed.
 */
uint8_t athread_create(athread_entry_t entry, void *info, uint16_t stack_size);

/**
 * Sets how many scheduler timer ticks a thread may run before the preemptive
 * scheduler rotates to the next ready thread.
 *
 * @param tid The thread ID to update.
 * @param quantum_ticks Number of timer ticks; values below 1 are clamped to 1.
 * @return 1 on success, 0 if the TID is invalid or unused.
 */
uint8_t athread_set_quantum(uint8_t tid, uint8_t quantum_ticks) __attribute__((no_instrument_function));

/**
 * Reads the configured quantum for a thread.
 *
 * @param tid The thread ID to inspect.
 * @return The thread quantum in scheduler timer ticks, or 0 for invalid/unused TIDs.
 */
uint8_t athread_get_quantum(uint8_t tid) __attribute__((no_instrument_function));

/**
 * Returns the number of allocated thread IDs, including the idle thread.
 */
uint8_t athread_get_thread_count(void) __attribute__((no_instrument_function));

/**
 * Copies cumulative scheduler timer ticks per thread into out_ticks.
 *
 * @param out_ticks Array with at least ATHREAD_MAX_THREADS elements.
 * @param max_ticks Number of elements available in out_ticks.
 * @return Number of copied thread counters.
 */
uint8_t athread_get_cpu_ticks(uint32_t *out_ticks, uint8_t max_ticks) __attribute__((no_instrument_function));

/**
 * Yields the currently running thread, allowing the scheduler to switch to another ready thread.
 */
void athread_yield(void);

/**
 * Puts the current thread to sleep for the given number of scheduler ticks.
 */
void athread_sleep_ticks(uint8_t ticks);

/**
 * Thread entry wrapper that is called when a thread starts running.
 */
void athread_bootstrap(void);

void athread_tick(void);

/**
 * Gets the thread ID of the currently running thread.
 * 
 * @note This function disables interrupts and re-enables them before returning.
 * @return The thread ID of the currently running thread.
 */
uint8_t athread_get_current_tid(void);

#endif // ATHREAD_H__
