# MegaRT

MegaRT is a small preemptive threading project for the Arduino Mega 2560
ATmega2560. The center of the project is `athread`: a tiny scheduler with AVR
context switching, timer-driven preemption, per-thread time quanta, sleeping
threads, and CPU accounting.

The repository also contains a hardware profiling demo built around that
scheduler: an SPI OLED process menu, a rotary encoder for changing thread
quanta at runtime, synthetic worker threads, and a Python Task Manager-style
viewer that graphs per-thread CPU usage over USART.

![MegaRT breadboard setup](images/breadboard.jpg)

![MegaRT Task Manager](images/task_manager.png)

## What It Does

- Runs multiple preemptive threads on an ATmega2560.
- Switches threads from a Timer1 interrupt using a small AVR assembly context
  switcher.
- Lets each thread have its own time quantum.
- Supports ready, running, blocked, sleeping, and zombie thread states.
- Tracks CPU ticks per thread.
- Displays running processes on a Waveshare 0.96 inch SPI OLED.
- Uses a rotary encoder to select a process and modify its quantum live.
- Streams CPU samples over USART.
- Shows live per-thread CPU graphs in a Tkinter desktop viewer.

## Hardware Demo

### OLED Wiring

Waveshare 0.96 inch OLED in 4-wire SPI mode:

| OLED pin | Mega 2560 pin |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| NC | Unconnected |
| DIN | D51 / MOSI |
| CLK | D52 / SCK |
| CS | D53 |
| DC | D49 |
| RES | D48 |
| BS0 | GND |
| BS1 | GND |

### Rotary Encoder Wiring

| Encoder pin | Mega 2560 pin |
| --- | --- |
| + | 5V |
| GND | GND |
| CLK | D43 |
| DT | D45 |
| SW | D47 |

## Thread Map

The demo firmware currently creates these threads:

| Thread | Name | Purpose |
| --- | --- | --- |
| T0 | IDLE | Scheduler idle thread |
| T1 | MAIN | Main application thread |
| T2 | ENC | Rotary encoder/menu input |
| T3 | OLED | OLED process menu |
| T4 | WRK1 | Demo worker workload |
| T5 | WRK2 | Demo worker workload |
| T6 | WRK3 | Demo worker workload |
| T7 | WRK4 | More dramatic demo workload |

## Project Layout

```text
include/
  athread/      Public scheduler API and generated AVR structure offsets
  platform/     USART, uptime, and debug headers
  profiling/    CPU stats, trace, and worker demo headers
  ui/           OLED and encoder headers

src/
  athread/      Scheduler implementation and AVR context switch assembly
  platform/     USART and millisecond uptime support
  profiling/    CPU sampling, trace hooks, and demo workloads
  ui/           SPI OLED driver and rotary encoder input
  main.c        Demo application setup

tools/
  cpu_task_manager.py       Tkinter CPU usage viewer
  run_cpu_task_manager.bat  Windows launcher using MSYS2 UCRT Python
  gen_offsets.py            PlatformIO pre-build offset generator
  gen_athread_offsets.c     Offset generator source
```

## Build And Upload

Build the firmware with PlatformIO:

```powershell
pio run
```

Upload to the Arduino Mega 2560:

```powershell
pio run -t upload
```

If the serial port is busy, close the CPU Task Manager or any serial monitor
before uploading.

## Run The CPU Viewer

After the firmware is running, start the Task Manager-style viewer:

```powershell
python ./tools/cpu_task_manager.py --port COM3
```

Change `COM3` if your Mega appears on another port.

## Scheduler Notes

- Timer1 drives preemption and quantum accounting.
- Timer2 drives millisecond uptime and encoder debouncing.
- `athread_set_quantum()` changes a thread's time slice at runtime.
- `athread_get_cpu_ticks()` exposes per-thread scheduler tick accounting.
- The profiling demo periodically sends compact CPU sample packets over USART.
- The OLED menu uses the scheduler API to display processes and edit quanta.

## Generated Files

The build generates AVR structure offsets for the assembly context switcher:

```text
include/athread/athread_offsets.h
```

This is produced by `tools/gen_offsets.py` from
`tools/gen_athread_offsets.c` before the firmware is compiled.

## Notes

Some AVR warnings about `__builtin_return_address` can appear when function
instrumentation is enabled. They are expected for the tracing/profiling hooks
used by the demo.

## License

MIT License. See `LICENSE` for details.
