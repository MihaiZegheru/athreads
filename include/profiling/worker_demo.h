#ifndef WORKER_DEMO_H__
#define WORKER_DEMO_H__

#include <stdint.h>

/**
 * Creates all demo worker threads used by the CPU usage graph.
 *
 * The workers simulate different kinds of process behavior: steady background
 * work, weighted compute, and a more dramatic workload for WRK4.
 */
void worker_demo_create_threads(void);

/**
 * Thread entry used by each demo worker.
 *
 * The info pointer must reference a uint8_t thread hint that selects the
 * worker profile to run.
 *
 * @param info Pointer to a uint8_t worker profile hint.
 */
void worker_demo_thread(void *info);

#endif // WORKER_DEMO_H__
