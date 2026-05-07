#ifndef CPU_STATS_H__
#define CPU_STATS_H__

/**
 * Sends one binary CPU usage sample over USART.
 *
 * The packet contains uptime, thread count, per-thread quantum values, and
 * scheduler tick deltas since the previous sample. Host tools use this stream
 * to render the Task Manager style graph.
 */
void cpu_stats_send(void);

#endif // CPU_STATS_H__
