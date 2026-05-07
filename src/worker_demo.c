#include "worker_demo.h"

#include "athread.h"
#include "encoder.h"
#include "worker_workload.h"
#include "uptime.h"

#include <avr/io.h>

static uint8_t s_worker1_tid_hint = 4;
static uint8_t s_worker2_tid_hint = 5;
static uint8_t s_worker3_tid_hint = 6;
static uint8_t s_worker4_tid_hint = 7;
static uint16_t s_worker_rng = 0xACE1u;

static uint16_t worker_runtime_noise(uint8_t tid_hint, uint16_t rng) {
    uint16_t noise = (uint16_t)TCNT1;
    noise ^= (uint16_t)((uint16_t)TCNT2 << 8);
    noise ^= (uint16_t)uptime_ms();
    noise ^= (uint16_t)encoder_get_value();
    noise ^= (uint16_t)((uint16_t)athread_get_current_tid() << 5);
    noise ^= (uint16_t)((uint16_t)tid_hint << 11);

    rng ^= (uint16_t)(noise + (uint16_t)(rng << 3) + (uint16_t)(rng >> 2));
    return (uint16_t)(rng * 25173u + 13849u);
}

void worker_demo_thread(void *info) {
    uint8_t tid_hint = *(uint8_t *)info;
    uint16_t rng = (uint16_t)(s_worker_rng ^ ((uint16_t)tid_hint << 8));
    s_worker_rng = (uint16_t)(s_worker_rng * 109u + 89u);
    volatile uint16_t sink = 0;

    while (1) {
        rng = worker_runtime_noise(tid_hint, rng);

        if (tid_hint == 4) {
            uint8_t run_ms = (uint8_t)(35u + (rng & 0x3Fu));
            uint8_t sleep_ticks = (uint8_t)(6u + ((rng >> 7) & 0x0Fu));
            uint32_t until_ms = uptime_ms() + run_ms;

            while ((int32_t)(uptime_ms() - until_ms) < 0) {
                rng = worker_runtime_noise(tid_hint, rng);
                sink ^= rng;
                worker_workload_run(tid_hint);
            }

            athread_sleep_ticks(sleep_ticks);
        } else if (tid_hint == 5) {
            uint8_t work_units = (uint8_t)(14u + (rng & 0x0Fu));

            for (uint8_t i = 0; i < work_units; i++) {
                rng = worker_runtime_noise(tid_hint, rng);
                sink ^= (uint16_t)(rng + i);
                worker_workload_run(tid_hint);
            }

            if ((rng & 0x00E0u) == 0x00A0u) {
                athread_sleep_ticks((uint8_t)(1u + ((rng >> 9) & 0x03u)));
            } else {
                athread_yield();
            }
        } else if (tid_hint == 6) {
            uint8_t work_units = (uint8_t)(16u + ((rng >> 3) & 0x13u));

            for (uint8_t i = 0; i < work_units; i++) {
                rng = worker_runtime_noise(tid_hint, rng);
                sink ^= (uint16_t)(rng ^ i);
                worker_workload_run(tid_hint);
            }

            if ((rng & 0x01C0u) == 0x0100u) {
                athread_sleep_ticks((uint8_t)(2u + ((rng >> 10) & 0x05u)));
            } else {
                athread_yield();
            }
        } else {
            uint8_t mode = (uint8_t)((rng ^ (uint16_t)(uptime_ms() >> 2)) & 0x07u);

            if (mode < 2) {
                athread_sleep_ticks((uint8_t)(12u + (rng & 0x1Fu)));
            } else if (mode < 5) {
                uint8_t bursts = (uint8_t)(2u + ((rng >> 5) & 0x07u));
                for (uint8_t burst = 0; burst < bursts; burst++) {
                    uint8_t work_units = (uint8_t)(4u + ((rng >> (burst & 0x07u)) & 0x0Fu));
                    for (uint8_t i = 0; i < work_units; i++) {
                        rng = worker_runtime_noise(tid_hint, rng);
                        sink ^= (uint16_t)(rng + ((uint16_t)burst << 5) + i);
                        worker_workload_run(6);
                    }
                    athread_yield();
                }
            } else if (mode < 7) {
                uint32_t until_ms = uptime_ms() + (uint8_t)(25u + (rng & 0x3Fu));
                while ((int32_t)(uptime_ms() - until_ms) < 0) {
                    rng = worker_runtime_noise(tid_hint, rng);
                    sink ^= (uint16_t)(rng ^ (uint16_t)TCNT1);
                    worker_workload_run(6);
                    if ((rng & 0x0030u) == 0x0010u) {
                        worker_workload_run(5);
                    }
                }
                athread_sleep_ticks((uint8_t)(1u + ((rng >> 9) & 0x03u)));
            } else {
                uint32_t until_ms = uptime_ms() + (uint8_t)(70u + (rng & 0x7Fu));
                while ((int32_t)(uptime_ms() - until_ms) < 0) {
                    rng = worker_runtime_noise(tid_hint, rng);
                    sink ^= (uint16_t)(rng + (uint16_t)TCNT2);
                    worker_workload_run(6);
                    worker_workload_run(5);
                }
                athread_sleep_ticks((uint8_t)(8u + ((rng >> 8) & 0x0Fu)));
            }
        }
    }
}

void worker_demo_create_threads(void) {
    uint8_t worker1_thread_tid = athread_create(worker_demo_thread, &s_worker1_tid_hint);
    uint8_t worker2_thread_tid = athread_create(worker_demo_thread, &s_worker2_tid_hint);
    uint8_t worker3_thread_tid = athread_create(worker_demo_thread, &s_worker3_tid_hint);
    uint8_t worker4_thread_tid = athread_create(worker_demo_thread, &s_worker4_tid_hint);

    (void)worker1_thread_tid;
    (void)worker2_thread_tid;
    (void)worker3_thread_tid;
    (void)worker4_thread_tid;
}
