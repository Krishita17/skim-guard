"""Code-generated diagrams that are not measurement charts:

* the system architecture block diagram  -> figures/architecture.*
* three Flipper UI-state mockups          -> figures/ui_*.*

These are drawn with matplotlib so they regenerate from a clean clone with no
extra tooling (no Node/mermaid-cli needed). The UI images are *mockups* of the
128x64 on-device screen, clearly labelled as such — they illustrate the states;
they are not photographs of hardware.

    python -m eval.make_diagrams

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

from eval import plots

FIG_DIR = "figures"


# --------------------------------------------------------------------------- #
# Architecture diagram
# --------------------------------------------------------------------------- #
def _box(ax, x, y, w, h, text, fc, ec=plots.INK):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08",
                                linewidth=1.4, edgecolor=ec, facecolor=fc))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=9, color=plots.INK)


def _arrow(ax, x1, y1, x2, y2):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>",
                                 mutation_scale=13, linewidth=1.3, color=plots.INK))


def architecture():
    fig, ax = plt.subplots(figsize=(9.2, 5.2))
    ax.set_xlim(0, 12); ax.set_ylim(0, 8); ax.axis("off")

    ax.text(6, 7.6, "SkimGuard — passive NFC reader detector (Flipper Zero)",
            ha="center", fontsize=13, fontweight="bold", color=plots.INK)

    _box(ax, 0.3, 4.3, 2.2, 1.1, "RF field from\nactive reader\n(external)", "#f2f4f7", plots.MUTED)
    _box(ax, 3.0, 4.3, 2.4, 1.1, "NFC field sensor\nfuri_hal_nfc\n(LISTEN ONLY)", "#e8f0fe")
    _box(ax, 5.9, 4.3, 2.4, 1.1, "Signal processor\nEMA smoothing\n+ trend", "#e8f0fe")
    _box(ax, 8.8, 4.3, 2.9, 1.1, "Detection logic\npresent / proximity\nsteady vs intermittent", "#e8f0fe")

    _box(ax, 3.0, 2.4, 2.4, 1.1, "Booth A/B mode\nclean vs\ncompromised", "#fef3e6")
    _box(ax, 5.9, 2.4, 2.4, 1.1, "UI + feedback\nEMF meter, clicks\nverdict, watch", "#fef3e6")
    _box(ax, 8.8, 2.4, 2.9, 1.1, "Logger + eval\nrange, FP rate\nfield-vs-distance", "#e6f6ef")

    _box(ax, 3.0, 0.6, 8.7, 1.0,
         "Off-device: sim/ trace generator  ·  eval/ studies  ·  figures/ charts  (no hardware needed)",
         "#f2f4f7", plots.MUTED)

    _arrow(ax, 2.5, 4.85, 3.0, 4.85)
    _arrow(ax, 5.4, 4.85, 5.9, 4.85)
    _arrow(ax, 8.3, 4.85, 8.8, 4.85)
    _arrow(ax, 10.2, 4.3, 7.1, 3.5)   # detection -> UI
    _arrow(ax, 7.1, 4.3, 7.1, 3.5)    # signal/detection -> UI
    _arrow(ax, 4.2, 3.5, 6.5, 3.5)    # A/B -> UI
    _arrow(ax, 10.2, 4.3, 10.2, 3.5)  # detection -> logger
    _arrow(ax, 10.2, 2.4, 7.35, 1.6)  # logger -> off-device

    ax.text(6, 0.2, "Never transmits — the app only senses fields other readers emit.",
            ha="center", fontsize=9, style="italic", color=plots.MUTED)

    return plots.save(fig, FIG_DIR, "architecture")


# --------------------------------------------------------------------------- #
# UI-state mockups (128 x 64 Flipper screen)
# --------------------------------------------------------------------------- #
def _screen(ax, title):
    # Flipper's LCD: dark ink on the characteristic amber background.
    bg, ink = "#ff9a1e", "#1b1b1f"
    ax.set_xlim(0, 128); ax.set_ylim(0, 64); ax.invert_yaxis(); ax.axis("off")
    ax.add_patch(plt.Rectangle((0, 0), 128, 64, facecolor=bg, edgecolor=ink, linewidth=2))
    ax.text(64, -6, title, ha="center", va="bottom", fontsize=9, color=plots.INK)
    return ink


def _meter(ax, ink, x, y, w, h, fill_frac, peak_frac=None):
    ax.add_patch(plt.Rectangle((x, y), w, h, fill=False, edgecolor=ink, linewidth=1.2))
    ax.add_patch(plt.Rectangle((x + 1, y + 1), (w - 2) * fill_frac, h - 2,
                               facecolor=ink, edgecolor="none"))
    if peak_frac is not None:
        px = x + 1 + (w - 2) * peak_frac
        ax.plot([px, px], [y - 2, y + h + 2], color=ink, lw=1.4)


def _waveform(ax, ink, x, y, w, h, samples):
    ax.plot([x, x + w], [y + h, y + h], color=ink, lw=0.8)  # baseline
    n = len(samples)
    for i, v in enumerate(samples):
        px = x + (w * i / max(1, n - 1))
        ax.plot([px, px], [y + h, y + h - h * v], color=ink, lw=1.0)


def _header(ax, ink, mode):
    ax.text(4, 11, "SkimGuard", fontsize=9.5, color=ink, fontweight="bold")
    ax.text(124, 11, mode, fontsize=8, color=ink, ha="right")
    ax.plot([0, 128], [14, 14], color=ink, lw=0.8)


def ui_states():
    import numpy as np
    paths = []

    # 1) Idle / quiet
    fig, ax = plt.subplots(figsize=(3.2, 1.9))
    ink = _screen(ax, "State: idle — verdict CLEAR")
    _header(ax, ink, "Sweep")
    ax.text(4, 24, "CLEAR", fontsize=7.5, color=ink)
    ax.text(124, 24, "c0%", fontsize=8, color=ink, ha="right")
    _waveform(ax, ink, 4, 27, 120, 12, [0.02] * 30)
    _meter(ax, ink, 4, 43, 120, 9, 0.03)
    ax.text(4, 61, "mute Med pk0%", fontsize=7, color=ink)
    paths += plots.save(fig, FIG_DIR, "ui_idle")

    # 2) Locked on
    fig, ax = plt.subplots(figsize=(3.2, 1.9))
    ink = _screen(ax, "State: ACTIVE — closing in")
    _header(ax, ink, "Sweep")
    ax.text(4, 24, "ACTIVE  warmer >>>", fontsize=7.5, color=ink, fontweight="bold")
    ax.text(124, 24, "c78%", fontsize=8, color=ink, ha="right", fontweight="bold")
    x = np.linspace(0, 1, 30)
    wave = 0.15 + 0.7 * x + 0.05 * np.sin(x * 22)
    _waveform(ax, ink, 4, 27, 120, 12, np.clip(wave, 0, 1))
    _meter(ax, ink, 4, 43, 120, 9, 0.78, peak_frac=0.82)
    ax.text(4, 61, "snd+led LOG Med pk82%", fontsize=6.3, color=ink)
    ax.text(124, 61, "~4cm", fontsize=6.5, color=ink, ha="right")
    paths += plots.save(fig, FIG_DIR, "ui_locked")

    # 3) Verdict / watch alert
    fig, ax = plt.subplots(figsize=(3.2, 1.9))
    ink = _screen(ax, "State: watch alert")
    _header(ax, ink, "Watch")
    ax.text(4, 26, "! READER DETECTED", fontsize=9, color=ink, fontweight="bold")
    ax.text(4, 37, "appeared while armed", fontsize=7.5, color=ink)
    _meter(ax, ink, 4, 42, 120, 8, 0.9, peak_frac=0.94)
    ax.text(4, 61, "snd+led pk94% armed", fontsize=7, color=ink)
    paths += plots.save(fig, FIG_DIR, "ui_verdict")

    return paths


def main():
    plots.apply_style()
    architecture()
    ui_states()
    print("[diagrams] wrote architecture + UI mockups to figures/")


if __name__ == "__main__":
    main()
