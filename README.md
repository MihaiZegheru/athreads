# MegaRT

MegaRT is a lightweight preemptive threading library for the ATmega2560 microcontroller, designed for real-time embedded applications. It features a custom thread scheduler, function-level profiling, and flame graph visualization.

## Features
- **Preemptive Thread Scheduler:** Context switching and thread management using AVR assembly and C.
- **Thread States:** Supports READY, RUNNING, BLOCKED, SLEEPING, ZOMBIE.
- **Timer-based Preemption:** Uses Timer1 for thread switching and Timer2 for millisecond uptime tracking.
- **UART Logging:** Debug output and trace export via serial.
- **Function Instrumentation:** GCC `-finstrument-functions` hooks for tracing function entry/exit.
- **Per-thread Profiling:** Each thread’s function calls are traced with timestamps and thread IDs.
- **Flame Graph Generation:** Profiling data is exported, processed by Python tools, and visualized as SVG flame graphs.

## Project Structure
- `src/` — Core library, scheduler, demo workloads, instrumentation.
- `include/` — Public headers.
- `lib/FlameGraph/` — Flame graph scripts (Brendan Gregg).
- `tools/` — Python trace reader and symbol resolver.
- `trace_output/` — CSV and SVG flame graph outputs.

## Typical Threads
- **Main thread:** Initializes system, schedules demo threads.
- **Worker threads:** Run demo pipelines with unique call patterns.
- **Tracer thread:** Flushes trace buffer to UART for offline analysis.

## Profiling & Flame Graphs
- Instrumentation hooks record function entry/exit, timestamp, and thread ID.
- Tracing buffer is periodically flushed by a dedicated tracer thread.
- Python tool (`tools/trace_reader.py`) reads trace data, resolves function names, and generates per-thread flame graphs.
- Flame graphs show time-weighted call stacks for each thread, helping identify hotspots and performance bottlenecks.

## Getting Started
1. Build and upload firmware using PlatformIO.
2. Run the Python trace reader to capture and process trace data.
3. View generated flame graphs in `trace_output/`.

## License
MIT License. See LICENSE for details.

---
For more details, see the source code and comments in each module.
