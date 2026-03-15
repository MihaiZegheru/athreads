import csv
import serial
import struct
import subprocess
from pathlib import Path
from bisect import bisect_right
from collections import Counter, defaultdict
from datetime import datetime
import tempfile

PORT = "COM3"
BAUD = 115200

PROJECT_ROOT = Path(__file__).resolve().parents[1]
ELF = PROJECT_ROOT / ".pio" / "build" / "megaatmega2560" / "firmware.elf"
AVR_NM = Path.home() / ".platformio" / "packages" / "toolchain-atmelavr" / "bin" / "avr-nm.exe"
FLAMEGRAPH_PL = PROJECT_ROOT / "lib" / "FlameGraph" / "flamegraph.pl"
PERL = "perl"  # or r"C:\Strawberry\perl\bin\perl.exe"

# C side:
# typedef struct __attribute__((packed)) {
#     uint32_t timestamp;
#     uintptr_t fn;
#     uint8_t type;
#     uint8_t tid;
# } trace_event_t;
EVENT_FMT = "<IHBB"   # timestamp, fn, type, tid
EVENT_SIZE = struct.calcsize(EVENT_FMT)

TYPE_ENTER = 1
TYPE_EXIT = 2

# Convert time into pseudo-samples so the flame graph looks less blocky.
# Tweak this. Smaller = more detail, larger = smoother / fewer samples.
SAMPLE_QUANTUM = 3  # timestamp units, likely ms in your setup

symbols = []
symbol_addrs = []

THREAD_NAMES = {
    0: "idle",
    1: "main",
    2: "tracer",
    3: "worker1",
    4: "worker2",
    5: "worker3",
}


def load_symbols():
    global symbols, symbol_addrs

    if not ELF.exists():
        raise FileNotFoundError(f"ELF not found: {ELF}")

    if not AVR_NM.exists():
        raise FileNotFoundError(f"avr-nm not found: {AVR_NM}")

    result = subprocess.run(
        [str(AVR_NM), "-n", "-C", str(ELF)],
        capture_output=True,
        text=True,
        check=True,
    )

    parsed = []

    for line in result.stdout.splitlines():
        parts = line.strip().split(maxsplit=2)
        if len(parts) < 3:
            continue

        addr_str, sym_type, name = parts

        if sym_type.lower() not in ("t", "w"):
            continue

        try:
            addr = int(addr_str, 16)
        except ValueError:
            continue

        # avr-nm gives byte addresses; trace stores code addresses in word-ish form
        word_addr = addr // 2
        parsed.append((word_addr, name))

    parsed.sort(key=lambda x: x[0])
    symbols = parsed
    symbol_addrs = [addr for addr, _ in parsed]


def resolve_symbol(addr: int) -> str:
    if not symbols:
        return "??"

    idx = bisect_right(symbol_addrs, addr) - 1
    if idx < 0:
        return f"??@0x{addr:04x}"

    sym_addr, sym_name = symbols[idx]
    offset = addr - sym_addr

    if offset == 0:
        return sym_name
    return f"{sym_name}+0x{offset:x}"


def event_type_name(t: int) -> str:
    if t == TYPE_ENTER:
        return "ENTER"
    if t == TYPE_EXIT:
        return "EXIT"
    return str(t)


def sanitize_symbol(name: str) -> str:
    return name.replace(";", ":").replace("\n", " ").strip()


def thread_label(tid: int) -> str:
    return THREAD_NAMES.get(tid, f"tid{tid}")


def save_csv(events, out_csv: Path):
    with out_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "timestamp",
            "tid",
            "thread",
            "type",
            "fn",
            "fn_name",
        ])

        for e in events:
            writer.writerow([
                e["timestamp"],
                e["tid"],
                thread_label(e["tid"]),
                event_type_name(e["type"]),
                f"0x{e['fn']:04x}",
                e["fn_name"],
            ])


def build_folded_per_thread(events):
    """
    Build time-weighted folded stacks per thread using ENTER/EXIT timestamps.

    To make the output look less blocky, durations are converted into
    pseudo-samples using SAMPLE_QUANTUM.

    Each live frame stores:
      - fn
      - start timestamp
      - child time accumulated under it

    On EXIT:
      total = exit - start
      self  = total - child
      samples = max(1, self // SAMPLE_QUANTUM)
      folded[stack] += samples
    """
    live_stacks = defaultdict(list)          # tid -> list[{fn,start,child}]
    folded_per_tid = defaultdict(Counter)    # tid -> Counter
    mismatch_count = 0

    for e in events:
        tid = e["tid"]
        fn_name = sanitize_symbol(e["fn_name"])
        ts = e["timestamp"]

        if e["type"] == TYPE_ENTER:
            live_stacks[tid].append({
                "fn": fn_name,
                "start": ts,
                "child": 0,
            })

        elif e["type"] == TYPE_EXIT:
            if not live_stacks[tid]:
                mismatch_count += 1
                continue

            top = live_stacks[tid][-1]

            if top["fn"] == fn_name:
                frame = live_stacks[tid].pop()

                total = ts - frame["start"]
                if total < 1:
                    total = 1

                self_time = total - frame["child"]
                if self_time < 1:
                    self_time = 1

                samples = max(1, self_time // SAMPLE_QUANTUM)

                stack = [f["fn"] for f in live_stacks[tid]] + [frame["fn"]]
                if stack:
                    folded_per_tid[tid][";".join(stack)] += samples

                if live_stacks[tid]:
                    live_stacks[tid][-1]["child"] += total

            else:
                # Mismatch recovery: search backward for matching frame.
                found_idx = None
                for i in range(len(live_stacks[tid]) - 1, -1, -1):
                    if live_stacks[tid][i]["fn"] == fn_name:
                        found_idx = i
                        break

                if found_idx is None:
                    mismatch_count += 1
                    continue

                frame = live_stacks[tid][found_idx]
                total = ts - frame["start"]
                if total < 1:
                    total = 1

                child_time = frame["child"]
                self_time = total - child_time
                if self_time < 1:
                    self_time = 1

                samples = max(1, self_time // SAMPLE_QUANTUM)

                stack = [f["fn"] for f in live_stacks[tid][:found_idx]] + [frame["fn"]]
                if stack:
                    folded_per_tid[tid][";".join(stack)] += samples

                # Collapse everything above and including the recovered frame
                live_stacks[tid] = live_stacks[tid][:found_idx]

    return folded_per_tid, mismatch_count


def generate_svg_from_counter(counter: Counter, out_svg: Path, title: str):
    flamegraph_path = Path(FLAMEGRAPH_PL)
    if not flamegraph_path.exists():
        raise FileNotFoundError(f"flamegraph.pl not found: {flamegraph_path}")

    with tempfile.NamedTemporaryFile("w", delete=False, encoding="utf-8", suffix=".folded") as tmp:
        tmp_path = Path(tmp.name)
        for stack, count in counter.items():
            if stack:
                tmp.write(f"{stack} {count}\n")

    try:
        with out_svg.open("w", encoding="utf-8") as svg_file:
            subprocess.run(
                [
                    PERL,
                    str(flamegraph_path),
                    "--color=hot",
                    "--minwidth", "0.5",
                    "--title", title,
                    "--width", "1800",
                    str(tmp_path),
                ],
                check=True,
                stdout=svg_file,
                text=True,
            )
    finally:
        try:
            tmp_path.unlink()
        except FileNotFoundError:
            pass


def main():
    load_symbols()
    print(f"Loaded {len(symbols)} symbols from {ELF}")
    print(f"Using SAMPLE_QUANTUM={SAMPLE_QUANTUM}")

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = PROJECT_ROOT / "trace_output"
    out_dir.mkdir(parents=True, exist_ok=True)

    out_csv = out_dir / f"trace_{timestamp}.csv"

    print("Capturing trace. Press Ctrl+C to stop.")
    print(f"CSV will be written to: {out_csv}")

    ser = serial.Serial(PORT, BAUD, timeout=1)
    events = []

    try:
        while True:
            start = ser.read(1)
            if not start:
                continue

            if start[0] != 0xAA:
                continue

            payload = ser.read(EVENT_SIZE)
            if len(payload) != EVENT_SIZE:
                print(f"[short packet: got {len(payload)} bytes, expected {EVENT_SIZE}]")
                continue

            ts, fn, typ, tid = struct.unpack(EVENT_FMT, payload)

            event = {
                "timestamp": ts,
                "fn": fn,
                "type": typ,
                "tid": tid,
                "fn_name": resolve_symbol(fn),
            }
            events.append(event)

            if len(events) % 500 == 0:
                print(
                    f"events={len(events)} "
                    f"last: t={ts} tid={tid} {event_type_name(typ)} {event['fn_name']}"
                )

    except KeyboardInterrupt:
        print("\nStopping capture...")

    finally:
        ser.close()

    print(f"Captured {len(events)} events")

    save_csv(events, out_csv)
    print(f"Raw CSV written to {out_csv}")

    folded_per_tid, mismatch_count = build_folded_per_thread(events)

    if mismatch_count:
        print(f"Stack mismatches detected: {mismatch_count}")

    if not folded_per_tid:
        print("No per-thread stacks were reconstructed.")
        return

    print("Generating per-thread SVGs...")

    for tid in sorted(folded_per_tid.keys()):
        label = thread_label(tid)
        out_svg = out_dir / f"trace_{timestamp}_{label}.svg"
        title = f"MegaRT Flame Graph - {label} (tid={tid})"

        try:
            generate_svg_from_counter(folded_per_tid[tid], out_svg, title)
            print(f"SVG written to {out_svg}")
        except Exception as e:
            print(f"Failed to generate SVG for tid={tid}: {e}")


if __name__ == "__main__":
    main()