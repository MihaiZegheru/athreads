#ifndef FLAME_DEMO_H__
#define FLAME_DEMO_H__

#include <stdint.h>

/**
 * Simulates work done by a thread in the flame demo.
 * 
 * @param tid The thread ID, which can be used to vary the work done by different threads.
 */
void flame_demo_work(uint8_t tid);

#endif // FLAME_DEMO_H__
