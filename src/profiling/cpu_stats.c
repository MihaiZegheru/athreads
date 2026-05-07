#include "profiling/cpu_stats.h"

#include "athread/athread.h"
#include "platform/uptime.h"
#include "platform/usart.h"

#include <stdint.h>

#define CPU_STATS_MARKER 0xCC

static void send_u8(uint8_t value) {
    usart_send_bytes(&value, 1);
}

static void send_u32(uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24)
    };
    usart_send_bytes(bytes, sizeof(bytes));
}

void cpu_stats_send(void) {
    static uint32_t previous_ticks[ATHREAD_MAX_THREADS] = {0};
    uint32_t ticks[ATHREAD_MAX_THREADS];
    uint8_t count = athread_get_cpu_ticks(ticks, ATHREAD_MAX_THREADS);

    send_u8(CPU_STATS_MARKER);
    send_u32(uptime_ms());
    send_u8(count);

    for (uint8_t tid = 0; tid < count; tid++) {
        uint32_t delta = ticks[tid] - previous_ticks[tid];
        previous_ticks[tid] = ticks[tid];

        send_u8(tid);
        send_u8(athread_get_quantum(tid));
        send_u32(delta);
    }
}
