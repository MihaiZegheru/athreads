#ifndef WORKER_WORKLOAD_H__
#define WORKER_WORKLOAD_H__

#include <stdint.h>

/**
 * Simulates CPU work done by a demo worker thread.
 * 
 * @param tid The thread ID, which can be used to vary the work done by different workers.
 */
void worker_workload_run(uint8_t tid);

#endif // WORKER_WORKLOAD_H__
