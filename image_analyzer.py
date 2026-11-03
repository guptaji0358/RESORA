"""Image Resolution Analyzer.

Scans a folder for images, reads their pixel dimensions from file headers
(no pixel decoding), classifies them by resolution and shows/exports results.
Original files are only ever opened read-only.
"""
from __future__ import annotations

import csv
import math
import os
import queue
import threading
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterator

from PIL import Image, UnidentifiedImageError

# --------------------------------------------------------------------------
# Classification
# --------------------------------------------------------------------------

# (label, width, height) ordered ascending. Dimensions are orientation-neutral
# (long side x short side), so portrait photos are classified like landscape.
TIERS: list[tuple[str, int, int]] = [
    ("SD", 640, 480),
    ("HD", 1280, 720),
    ("Full HD", 1920, 1080),
    ("QHD / 1440p", 2560, 1440),
    ("4K UHD", 3840, 2160),
    ("5K", 5120, 2880),
    ("8K UHD", 7680, 4320),
]
BELOW_SD = "Below SD"
ABOVE_8K = "Above 8K"
CLASS_ORDER: list[str] = [BELOW_SD] + [t[0] for t in TIERS] + [ABOVE_8K]

CLASSIFICATION_HELP = (
    "How images are classified (actual pixel size, after EXIF rotation;\n"
    "orientation is ignored, so portrait = landscape):\n\n"
    "- An image gets the HIGHEST tier whose width AND height it both reaches.\n"
    "  e.g. 4000x3000 -> 4K UHD (>= 3840x2160); 2560x1080 -> Full HD\n"
    "  (width reaches QHD, height does not).\n"
    "- Smaller than 640x480 in either dimension -> Below SD.\n"
    "- Larger than 7680x4320 in either dimension (while still meeting it\n"
    "  in both) -> Above 8K.\n"
    "- Exact matches are plain labels; anything else is non-standard but\n"
    "  still placed in the tier it fully reaches.\n"
    "- 2048x1080 is shown as 'Full HD (DCI 2K)': it is the cinema 2K format,\n"
    "  not QHD 2560x1440.\n"
    "- Aspect ratios are not assumed to be 16:9; unusual ones are flagged."
)

# Well-known ratios (w/h, label), landscape form.
KNOWN_RATIOS: list[tuple[float, str]] = [
    (1.0, "1:1"), (5 / 4, "5:4"), (4 / 3, "4:3"), (3 / 2, "3:2"),
    (16 / 10, "16:10"), (16 / 9, "16:9"), (1.85, "1.85:1"), (2.0, "2:1"),
    (21 / 9, "21:9"), (2.39, "2.39:1"), (32 / 9, "32:9"),
]
RATIO_TOLERANCE = 0.01  # 1 %


def classify(width: int, height: int) -> str:
    """Return the resolution class for the given pixel dimensions."""
    long_side, short_side = max(width, height), min(width, height)
    if (long_side, short_side) == (2048, 1080):
        return "Full HD (DCI 2K)"
    label = BELOW_SD
    for name, w, h in TIERS:
        if long_side >= w and short_side >= h:
            label = name
    if label == "8K UHD" and (long_side > 7680 or short_side > 4320):
        return ABOVE_8K
    return label


def class_rank(label: str) -> int:
    base = label.split(" (")[0]
    return CLASS_ORDER.index(base) if base in CLASS_ORDER else -1


def aspect_ratio(width: int, height: int) -> str:
    """Human-readable aspect ratio; unusual ratios are flagged."""
    if width <= 0 or height <= 0:
        return "n/a"
    portrait = height > width
    long_side, short_side = max(width, height), min(width, height)
    ratio = long_side / short_side
    for value, name in KNOWN_RATIOS:
        if abs(ratio - value) / value <= RATIO_TOLERANCE:
            a, b = name.split(":")
            return f"{b}:{a} (portrait)" if portrait and a != b else name
    g = math.gcd(width, height)
    exact = f"{width // g}:{height // g}"
    text = exact if max(width, height) // g <= 30 else f"{ratio:.2f}:1"
    if portrait:
        text = f"1:{ratio:.2f}" if text.endswith(":1") else text
    kind = "ultrawide" if ratio >= 2.2 else "unusual"
    return f"{text} ({kind}{', portrait' if portrait else ''})"


# --------------------------------------------------------------------------
# Scanning
# --------------------------------------------------------------------------

_NOT_IMAGES = {".pdf", ".eps", ".ps"}


def supported_extensions() -> frozenset[str]:
    Image.init()
    return frozenset(
        ext.lower() for ext, fmt in Image.registered_extensions().items()
        if fmt in Image.OPEN and ext.lower() not in _NOT_IMAGES
    )


@dataclass(frozen=True)
class ImageInfo:
    filename: str
    width: int
    height: int
    classification: str
    fmt: str
    size_mb: float
    aspect: str
    path: str


@dataclass(frozen=True)
class FailedImage:
    path: str
    reason: str


def analyze_image(path: Path) -> ImageInfo:
    """Read metadata only (Image.open is lazy); never writes to the file."""
    with Image.open(path) as img:
        width, height = img.size
        fmt = img.format or path.suffix.lstrip(".").upper()
        try:
            orientation = img.getexif().get(0x0112, 1)
        except Exception:  # damaged EXIF must not fail the image
            orientation = 1
    if orientation in (5, 6, 7, 8):
        width, height = height, width
    size_mb = path.stat().st_size / (1024 * 1024)
    return ImageInfo(path.name, width, height, classify(width, height), fmt,
                     round(size_mb, 3), aspect_ratio(width, height), str(path))


def find_candidates(folder: Path, recursive: bool,
                    stop: threading.Event | None = None) -> list[Path]:
    exts = supported_extensions()
    found: list[Path] = []
    for root, dirs, files in os.walk(folder):
        if stop is not None and stop.is_set():
            break
        if not recursive:
            dirs.clear()
        for name in files:
            if Path(name).suffix.lower() in exts:
                found.append(Path(root) / name)
    return found


def scan_folder(folder: Path, recursive: bool, stop: threading.Event,
                ) -> Iterator[tuple[int, int, ImageInfo | FailedImage]]:
    """Yield (done, total, result) for each candidate file."""
    candidates = find_candidates(folder, recursive, stop)
    total = len(candidates)
    if total == 0:
        yield 0, 0, FailedImage("", "")  # sentinel handled by caller
        return
    for i, path in enumerate(candidates, 1):
        if stop.is_set():
            return
        try:
            yield i, total, analyze_image(path)
        except (UnidentifiedImageError, OSError, ValueError, SyntaxError,
                Image.DecompressionBombError, MemoryError) as exc:
            yield i, total, FailedImage(str(path), f"{type(exc).__name__}: {exc}")


# --------------------------------------------------------------------------
# Summary and export
# --------------------------------------------------------------------------

def summarize(results: list[ImageInfo], failed: list[FailedImage]) -> dict:
    counts = {name: 0 for name in CLASS_ORDER}
    for r in results:
        counts[r.classification.split(" (")[0]] += 1
    best = max(results, key=lambda r: r.width * r.height, default=None)
    n = len(results)
    return {
        "found": n + len(failed),
        "analyzed": n,
        "failed": len(failed),
        "counts": counts,
        "highest": best,
        "avg_w": sum(r.width for r in results) / n if n else 0.0,
        "avg_h": sum(r.height for r in results) / n if n else 0.0,
    }


def summary_lines(s: dict) -> list[str]:
    c = s["counts"]
    best: ImageInfo | None = s["highest"]
    highest = (f"{best.filename} ({best.width}x{best.height})" if best else "-")
    avg = f"{s['avg_w']:.0f} x {s['avg_h']:.0f}" if s["analyzed"] else "-"
    return [
        f"Total images found: {s['found']}",
        f"Successfully analyzed: {s['analyzed']}",
        f"Failed / unreadable: {s['failed']}",
        f"HD: {c['HD']}",
        f"Full HD: {c['Full HD']}",
        f"QHD: {c['QHD / 1440p']}",
        f"4K: {c['4K UHD']}",
        f"5K: {c['5K']}",
        f"8K: {c['8K UHD']}",
        f"Above 8K: {c[ABOVE_8K]}",
        f"Highest resolution: {highest}",
        f"Average dimensions: {avg}",
    ]


CSV_HEADERS = ["Filename", "Width (px)", "Height (px)", "Resolution",
               "Format", "File size (MB)", "Aspect ratio", "Full path"]


def _row(r: ImageInfo) -> list[object]:
    return [r.filename, r.width, r.height, r.classification, r.fmt,
            f"{r.size_mb:.2f}", r.aspect, r.path]


def export_csv(path: Path, results: list[ImageInfo]) -> None:
    # utf-8-sig so Excel on Windows opens non-ASCII filenames correctly.
    with open(path, "w", newline="", encoding="utf-8-sig") as fh:
        writer = csv.writer(fh)
        writer.writerow(CSV_HEADERS)
        writer.writerows(_row(r) for r in results)


def export_txt(path: Path, results: list[ImageInfo],
               failed: list[FailedImage], folder: str = "") -> None:
    lines = ["IMAGE RESOLUTION REPORT", "=" * 70,
             f"Generated: {datetime.now():%Y-%m-%d %H:%M:%S}"]
    if folder:
        lines.append(f"Folder:    {folder}")
    lines += ["", "SUMMARY", "-" * 70, *summary_lines(summarize(results, failed)),
              "", "CLASSIFICATION RULES", "-" * 70, CLASSIFICATION_HELP, "",
              "IMAGES", "-" * 70]
    for i, r in enumerate(results, 1):
        lines += [f"{i}. {r.filename}",
                  f"     Size:   {r.width} x {r.height} px   Class: {r.classification}",
                  f"     Format: {r.fmt}   File size: {r.size_mb:.2f} MB   "
                  f"Aspect: {r.aspect}",
                  f"     Path:   {r.path}"]
    if failed:
        lines += ["", "FAILED / UNREADABLE FILES", "-" * 70]
        lines += [f"{f.path}\n     {f.reason}" for f in failed]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------
# GUI
# --------------------------------------------------------------------------

def run_gui() -> None:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    class App(tk.Tk):
        COLUMNS = [  # (heading, width, anchor)
            ("Filename", 200, "w"), ("Width", 70, "e"), ("Height", 70, "e"),
            ("Resolution", 130, "w"), ("Format", 70, "center"),
            ("Size (MB)", 80, "e"), ("Aspect ratio", 130, "w"),
            ("Full path", 420, "w"),
        ]

        def __init__(self) -> None:
            super().__init__()
            self.title("Image Resolution Analyzer")
            self.geometry("1180x720")
            self.minsize(900, 560)
            self.folder = tk.StringVar()
            self.recursive = tk.BooleanVar(value=True)
            self.status = tk.StringVar(value="Choose a folder to begin.")
            self.counter = tk.StringVar(value="Scanned: 0 | Failed: 0")
            self.results: list[ImageInfo] = []
            self.failed: list[FailedImage] = []
            self.msgs: queue.Queue = queue.Queue()
            self.stop_event = threading.Event()
            self.worker: threading.Thread | None = None
            self.sort_state: tuple[int, bool] | None = None
            self._style()
            self._build()
            self._refresh_summary()
            self.protocol("WM_DELETE_WINDOW", self._on_close)
            self.after(50, self._poll)

        # ---- layout
        def _style(self) -> None:
            st = ttk.Style(self)
            st.theme_use("clam")
            st.configure("Treeview", rowheight=24, font=("Segoe UI", 9))
            st.configure("Treeview.Heading", font=("Segoe UI", 9, "bold"))
            st.configure("Accent.TButton", font=("Segoe UI", 9, "bold"))
            st.configure("Card.TLabelframe.Label", font=("Segoe UI", 10, "bold"))

        def _build(self) -> None:
            pad = {"padx": 10, "pady": 5}
            top = ttk.Frame(self)
            top.pack(fill="x", **pad)
            ttk.Button(top, text="Browse Folder", command=self.browse).pack(side="left")
            ttk.Entry(top, textvariable=self.folder).pack(
                side="left", fill="x", expand=True, padx=8)
            ttk.Checkbutton(top, text="Include subfolders",
                            variable=self.recursive).pack(side="left")

            bar = ttk.Frame(self)
            bar.pack(fill="x", **pad)
            self.btn_start = ttk.Button(bar, text="Start Scan", style="Accent.TButton",
                                        command=self.start_scan)
            self.btn_start.pack(side="left")
            self.btn_stop = ttk.Button(bar, text="Stop Scan", state="disabled",
                                       command=self.stop_scan)
            self.btn_stop.pack(side="left", padx=6)
            self.btn_csv = ttk.Button(bar, text="Export CSV",
                                      command=lambda: self.export("csv"))
            self.btn_csv.pack(side="left", padx=(16, 0))
            self.btn_txt = ttk.Button(bar, text="Export TXT",
                                      command=lambda: self.export("txt"))
            self.btn_txt.pack(side="left", padx=6)
            ttk.Button(bar, text="Classification rules",
                       command=lambda: messagebox.showinfo(
                           "Classification rules", CLASSIFICATION_HELP)
                       ).pack(side="right")

            prog = ttk.Frame(self)
            prog.pack(fill="x", **pad)
            self.progress = ttk.Progressbar(prog, mode="determinate")
            self.progress.pack(side="left", fill="x", expand=True)
            ttk.Label(prog, textvariable=self.counter, width=30,
                      anchor="e").pack(side="left", padx=(8, 0))

            body = ttk.PanedWindow(self, orient="horizontal")
            body.pack(fill="both", expand=True, **pad)

            tf = ttk.Frame(body)
            body.add(tf, weight=4)
            cols = [c[0] for c in self.COLUMNS]
            self.tree = ttk.Treeview(tf, columns=cols, show="headings",
                                     selectmode="browse")
            for i, (name, width, anchor) in enumerate(self.COLUMNS):
                self.tree.heading(name, text=name,
                                  command=lambda i=i: self.sort_by(i))
                self.tree.column(name, width=width, anchor=anchor, stretch=False)
            vs = ttk.Scrollbar(tf, orient="vertical", command=self.tree.yview)
            hs = ttk.Scrollbar(tf, orient="horizontal", command=self.tree.xview)
            self.tree.configure(yscrollcommand=vs.set, xscrollcommand=hs.set)
            self.tree.grid(row=0, column=0, sticky="nsew")
            vs.grid(row=0, column=1, sticky="ns")
            hs.grid(row=1, column=0, sticky="ew")
            tf.rowconfigure(0, weight=1)
            tf.columnconfigure(0, weight=1)

            card = ttk.LabelFrame(body, text="Summary", style="Card.TLabelframe",
                                  padding=10)
            body.add(card, weight=1)
            self.summary = ttk.Label(card, justify="left", wraplength=240)
            self.summary.pack(anchor="nw")

            ttk.Label(self, textvariable=self.status, relief="sunken",
                      anchor="w", padding=(8, 2)).pack(fill="x", side="bottom")

        # ---- actions
        def browse(self) -> None:
            chosen = filedialog.askdirectory(title="Select image folder")
            if chosen:
                self.folder.set(os.path.normpath(chosen))

        def start_scan(self) -> None:
            folder = Path(self.folder.get().strip())
            if not folder.is_dir():
                messagebox.showwarning("Folder", "Please choose a valid folder.")
                return
            self._clear()
            self.stop_event.clear()
            self._set_scanning(True)
            self.status.set("Scanning...")
            self.progress.configure(mode="indeterminate")
            self.progress.start(12)
            self.worker = threading.Thread(
                target=self._work, args=(folder, self.recursive.get()), daemon=True)
            self.worker.start()

        def stop_scan(self) -> None:
            self.stop_event.set()
            self.status.set("Stopping...")

        def _work(self, folder: Path, recursive: bool) -> None:
            try:
                for done, total, res in scan_folder(folder, recursive, self.stop_event):
                    self.msgs.put(("item", done, total, res))
            except Exception as exc:  # never let the worker die silently
                self.msgs.put(("error", str(exc)))
            self.msgs.put(("done", self.stop_event.is_set()))

        def _poll(self) -> None:
            added = False
            for _ in range(60):  # bounded batch keeps the UI responsive
                try:
                    msg = self.msgs.get_nowait()
                except queue.Empty:
                    break
                kind = msg[0]
                if kind == "item":
                    _, done, total, res = msg
                    if total == 0:
                        continue
                    if self.progress["mode"] != "determinate":
                        self.progress.stop()
                        self.progress.configure(mode="determinate", maximum=total)
                    self.progress["value"] = done
                    if isinstance(res, ImageInfo):
                        self.results.append(res)
                        self._insert(len(self.results) - 1, res)
                        added = True
                    else:
                        self.failed.append(res)
                    self.counter.set(f"Scanned: {done}/{total} | "
                                     f"Failed: {len(self.failed)}")
                elif kind == "error":
                    messagebox.showerror("Scan error", msg[1])
                elif kind == "done":
                    self._finish(msg[1])
            if added or self.worker is not None:
                self._refresh_summary()
            self.after(50, self._poll)

        def _finish(self, stopped: bool) -> None:
            self.progress.stop()
            if self.progress["mode"] == "indeterminate":
                self.progress.configure(mode="determinate", value=0)
            self._set_scanning(False)
            self.worker = None
            n = len(self.results)
            self.status.set(
                f"{'Stopped' if stopped else 'Finished'}: {n} analyzed, "
                f"{len(self.failed)} failed." + ("" if n or stopped else
                                                  " No supported images found."))
            self._refresh_summary()

        def _set_scanning(self, on: bool) -> None:
            self.btn_start.configure(state="disabled" if on else "normal")
            self.btn_stop.configure(state="normal" if on else "disabled")

        def _clear(self) -> None:
            self.tree.delete(*self.tree.get_children())
            self.results.clear()
            self.failed.clear()
            self.sort_state = None
            self.progress["value"] = 0
            self.counter.set("Scanned: 0 | Failed: 0")
            for name, *_ in self.COLUMNS:
                self.tree.heading(name, text=name)

        def _insert(self, idx: int, r: ImageInfo) -> None:
            self.tree.insert("", "end", iid=str(idx), values=(
                r.filename, r.width, r.height, r.classification, r.fmt,
                f"{r.size_mb:.2f}", r.aspect, r.path))

        def _refresh_summary(self) -> None:
            self.summary.configure(
                text="\n".join(summary_lines(summarize(self.results, self.failed))))

        # ---- sorting
        def sort_by(self, col: int) -> None:
            asc = not (self.sort_state and self.sort_state[0] == col
                       and self.sort_state[1])
            keys = [
                lambda r: r.filename.lower(), lambda r: r.width,
                lambda r: r.height, lambda r: (class_rank(r.classification),
                                               r.width * r.height),
                lambda r: r.fmt, lambda r: r.size_mb,
                lambda r: r.width / max(r.height, 1), lambda r: r.path.lower(),
            ]
            order = sorted(range(len(self.results)),
                           key=lambda i: keys[col](self.results[i]), reverse=not asc)
            for pos, iid in enumerate(order):
                self.tree.move(str(iid), "", pos)
            self.sort_state = (col, asc)
            for i, (name, *_) in enumerate(self.COLUMNS):
                arrow = (" ▲" if asc else " ▼") if i == col else ""
                self.tree.heading(name, text=name + arrow)

        # ---- export
        def export(self, kind: str) -> None:
            if not self.results and not self.failed:
                messagebox.showinfo("Export", "Nothing to export yet.")
                return
            path = filedialog.asksaveasfilename(
                defaultextension=f".{kind}",
                filetypes=[(kind.upper() + " file", f"*.{kind}")],
                initialfile=f"image_report.{kind}")
            if not path:
                return
            try:
                if kind == "csv":
                    export_csv(Path(path), self._ordered())
                else:
                    export_txt(Path(path), self._ordered(), self.failed,
                               self.folder.get())
            except OSError as exc:
                messagebox.showerror("Export failed", str(exc))
                return
            self.status.set(f"Saved {path}")

        def _ordered(self) -> list[ImageInfo]:
            """Results in the order currently shown in the table."""
            return [self.results[int(i)] for i in self.tree.get_children()]

        def _on_close(self) -> None:
            self.stop_event.set()
            self.destroy()

    App().mainloop()


if __name__ == "__main__":
    run_gui()
