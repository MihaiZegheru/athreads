import argparse
import struct
import threading
import time
import tkinter as tk
from collections import defaultdict, deque
from queue import Empty, Queue

import serial


MARKER = 0xCC
BAUD = 115200

THREAD_NAMES = {
    0: "IDLE",
    1: "MAIN",
    2: "ENC",
    3: "OLED",
    4: "WRK1",
    5: "WRK2",
    6: "WRK3",
    7: "WRK4",
}

THREAD_COLORS = {
    0: "#7b8a8b",
    1: "#3b82f6",
    2: "#f59e0b",
    3: "#10b981",
    4: "#ef4444",
    5: "#a855f7",
    6: "#14b8a6",
    7: "#eab308",
}


def thread_name(tid):
    return THREAD_NAMES.get(tid, f"T{tid}")


def thread_color(tid):
    return THREAD_COLORS.get(tid, "#a855f7")


def graph_draw_order(tids):
    return sorted(tids, key=lambda tid: 99 if tid == 4 else tid)


def read_exact(ser, size):
    data = ser.read(size)
    if len(data) != size:
        return None
    return data


def read_packet(ser):
    while True:
        marker = ser.read(1)
        if not marker:
            return None
        if marker[0] == MARKER:
            break

    header = read_exact(ser, 5)
    if header is None:
        return None

    uptime_ms, count = struct.unpack("<IB", header)
    if count > 16:
        return None

    records = []
    for _ in range(count):
        payload = read_exact(ser, 6)
        if payload is None:
            return None
        tid, quantum, ticks = struct.unpack("<BBI", payload)
        records.append((tid, quantum, ticks))

    total = sum(ticks for _, _, ticks in records)
    samples = []
    for tid, quantum, ticks in records:
        percent = (ticks * 100.0 / total) if total else 0.0
        samples.append({
            "tid": tid,
            "name": thread_name(tid),
            "quantum": quantum,
            "ticks": ticks,
            "percent": percent,
        })

    return {
        "uptime_ms": uptime_ms,
        "total_ticks": total,
        "samples": samples,
    }


class SerialReader(threading.Thread):
    def __init__(self, port, baud, output_queue, stop_event):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.output_queue = output_queue
        self.stop_event = stop_event

    def run(self):
        try:
            with serial.Serial(self.port, self.baud, timeout=1) as ser:
                self.output_queue.put(("status", f"Connected to {self.port} at {self.baud} baud"))
                while not self.stop_event.is_set():
                    packet = read_packet(ser)
                    if packet is not None:
                        self.output_queue.put(("packet", packet))
        except Exception as exc:
            self.output_queue.put(("status", f"Serial error: {exc}"))


class CpuTaskManagerApp:
    def __init__(self, root, port, baud):
        self.root = root
        self.root.title("MegaRT Task Manager")
        self.root.geometry("980x620")
        self.root.configure(bg="#101418")

        self.queue = Queue()
        self.stop_event = threading.Event()
        self.reader = SerialReader(port, baud, self.queue, self.stop_event)

        self.history_len = 240
        self.histories = defaultdict(lambda: deque(maxlen=self.history_len))
        self.latest = {}
        self.uptime_ms = 0
        self.total_ticks = 0
        self.status_text = tk.StringVar(value="Waiting for data...")

        self._build_ui()
        self.reader.start()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(50, self._poll_queue)

    def _build_ui(self):
        header = tk.Frame(self.root, bg="#101418")
        header.pack(fill=tk.X, padx=18, pady=(14, 8))

        title = tk.Label(
            header,
            text="MegaRT Task Manager",
            bg="#101418",
            fg="#f8fafc",
            font=("Segoe UI", 20, "bold"),
        )
        title.pack(side=tk.LEFT)

        self.summary = tk.Label(
            header,
            text="",
            bg="#101418",
            fg="#94a3b8",
            font=("Segoe UI", 11),
        )
        self.summary.pack(side=tk.RIGHT)

        main = tk.Frame(self.root, bg="#101418")
        main.pack(fill=tk.BOTH, expand=True, padx=18, pady=8)

        graph_frame = tk.Frame(main, bg="#151b22", highlightthickness=1, highlightbackground="#263241")
        graph_frame.pack(fill=tk.BOTH, expand=True)

        graph_title = tk.Label(
            graph_frame,
            text="CPU Usage History",
            bg="#151b22",
            fg="#e2e8f0",
            font=("Segoe UI", 13, "bold"),
            anchor="w",
        )
        graph_title.pack(fill=tk.X, padx=14, pady=(10, 4))

        self.canvas = tk.Canvas(graph_frame, height=300, bg="#0b1016", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=14, pady=(0, 14))

        lower = tk.Frame(main, bg="#101418")
        lower.pack(fill=tk.BOTH, pady=(12, 0))

        self.process_frame = tk.Frame(lower, bg="#151b22", highlightthickness=1, highlightbackground="#263241")
        self.process_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        process_title = tk.Label(
            self.process_frame,
            text="Processes",
            bg="#151b22",
            fg="#e2e8f0",
            font=("Segoe UI", 13, "bold"),
            anchor="w",
        )
        process_title.pack(fill=tk.X, padx=14, pady=(10, 4))

        self.rows_container = tk.Frame(self.process_frame, bg="#151b22")
        self.rows_container.pack(fill=tk.BOTH, expand=True, padx=14, pady=(0, 14))
        self.rows = {}

        legend = tk.Frame(lower, width=190, bg="#151b22", highlightthickness=1, highlightbackground="#263241")
        legend.pack(side=tk.RIGHT, fill=tk.Y, padx=(12, 0))
        legend.pack_propagate(False)

        legend_title = tk.Label(
            legend,
            text="Legend",
            bg="#151b22",
            fg="#e2e8f0",
            font=("Segoe UI", 13, "bold"),
            anchor="w",
        )
        legend_title.pack(fill=tk.X, padx=14, pady=(10, 4))

        self.legend_container = tk.Frame(legend, bg="#151b22")
        self.legend_container.pack(fill=tk.BOTH, expand=True, padx=14, pady=(0, 14))

        status = tk.Label(
            self.root,
            textvariable=self.status_text,
            bg="#101418",
            fg="#94a3b8",
            font=("Segoe UI", 10),
            anchor="w",
        )
        status.pack(fill=tk.X, padx=18, pady=(0, 12))

    def _poll_queue(self):
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "status":
                    self.status_text.set(payload)
                elif kind == "packet":
                    self._apply_packet(payload)
        except Empty:
            pass

        self._draw()
        self.root.after(100, self._poll_queue)

    def _apply_packet(self, packet):
        self.uptime_ms = packet["uptime_ms"]
        self.total_ticks = packet["total_ticks"]

        seen = set()
        for sample in packet["samples"]:
            tid = sample["tid"]
            seen.add(tid)
            self.latest[tid] = sample
            self.histories[tid].append(sample["percent"])

        for tid in list(self.latest):
            if tid not in seen:
                self.histories[tid].append(0.0)

    def _draw(self):
        self.summary.config(
            text=f"Uptime {self.uptime_ms / 1000.0:,.1f}s   Sample ticks {self.total_ticks}"
        )
        self._draw_history_graph()
        self._draw_process_rows()
        self._draw_legend()

    def _draw_history_graph(self):
        c = self.canvas
        c.delete("all")
        width = max(c.winfo_width(), 1)
        height = max(c.winfo_height(), 1)
        pad_l, pad_t, pad_r, pad_b = 44, 18, 12, 30
        x0, y0 = pad_l, pad_t
        x1, y1 = width - pad_r, height - pad_b

        c.create_rectangle(x0, y0, x1, y1, outline="#263241", fill="#0b1016")

        for pct in (0, 25, 50, 75, 100):
            y = y1 - (pct / 100.0) * (y1 - y0)
            c.create_line(x0, y, x1, y, fill="#1f2937")
            c.create_text(8, y, text=f"{pct}%", fill="#64748b", anchor="w", font=("Segoe UI", 8))

        tids = graph_draw_order(self.latest)
        if not tids:
            c.create_text(width / 2, height / 2, text="Waiting for USART CPU samples", fill="#64748b")
            return

        n = self.history_len
        plot_w = max(x1 - x0, 1)
        plot_h = max(y1 - y0, 1)

        for tid in tids:
            values = list(self.histories[tid])
            if len(values) < 2:
                continue

            points = []
            for i, value in enumerate(values):
                x = x0 + (i / max(len(values) - 1, 1)) * plot_w
                y = y1 - (value / 100.0) * plot_h
                points.extend((x, y))

            line_width = 3 if tid >= 4 else 1
            c.create_line(points, fill=thread_color(tid), width=line_width, smooth=False)

    def _ensure_row(self, tid):
        if tid in self.rows:
            return self.rows[tid]

        row = tk.Frame(self.rows_container, bg="#151b22")
        row.pack(fill=tk.X, pady=3)

        name = tk.Label(row, bg="#151b22", fg="#e2e8f0", width=8, anchor="w", font=("Segoe UI", 10, "bold"))
        name.pack(side=tk.LEFT)

        quantum = tk.Label(row, bg="#151b22", fg="#94a3b8", width=5, anchor="w", font=("Segoe UI", 10))
        quantum.pack(side=tk.LEFT)

        percent = tk.Label(row, bg="#151b22", fg="#e2e8f0", width=7, anchor="e", font=("Segoe UI", 10))
        percent.pack(side=tk.RIGHT)

        bar_canvas = tk.Canvas(row, height=16, bg="#0b1016", highlightthickness=1, highlightbackground="#263241")
        bar_canvas.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=8)

        self.rows[tid] = {
            "frame": row,
            "name": name,
            "quantum": quantum,
            "percent": percent,
            "bar": bar_canvas,
        }
        return self.rows[tid]

    def _draw_process_rows(self):
        for tid in sorted(self.latest):
            sample = self.latest[tid]
            row = self._ensure_row(tid)
            row["name"].config(text=f"T{tid} {sample['name']}")
            row["quantum"].config(text=f"Q{sample['quantum']}")
            row["percent"].config(text=f"{sample['percent']:5.1f}%")

            b = row["bar"]
            b.delete("all")
            w = max(b.winfo_width(), 1)
            h = max(b.winfo_height(), 1)
            fill_w = (sample["percent"] / 100.0) * w
            b.create_rectangle(0, 0, w, h, fill="#0b1016", outline="")
            b.create_rectangle(0, 0, fill_w, h, fill=thread_color(tid), outline="")

    def _draw_legend(self):
        for child in self.legend_container.winfo_children():
            child.destroy()

        for tid in sorted(self.latest):
            row = tk.Frame(self.legend_container, bg="#151b22")
            row.pack(fill=tk.X, pady=4)
            swatch = tk.Canvas(row, width=14, height=14, bg="#151b22", highlightthickness=0)
            swatch.pack(side=tk.LEFT)
            swatch.create_rectangle(2, 2, 12, 12, fill=thread_color(tid), outline="")
            label = tk.Label(
                row,
                text=f"T{tid} {thread_name(tid)}",
                bg="#151b22",
                fg="#cbd5e1",
                font=("Segoe UI", 10),
                anchor="w",
            )
            label.pack(side=tk.LEFT, padx=6)

    def close(self):
        self.stop_event.set()
        self.root.after(100, self.root.destroy)


def main():
    parser = argparse.ArgumentParser(description="MegaRT graphical CPU monitor.")
    parser.add_argument("--port", default="COM3", help="Serial port, for example COM3 or /dev/ttyACM0.")
    parser.add_argument("--baud", type=int, default=BAUD, help="Serial baud rate.")
    args = parser.parse_args()

    root = tk.Tk()
    CpuTaskManagerApp(root, args.port, args.baud)
    root.mainloop()


if __name__ == "__main__":
    main()
