"""Matplotlib helpers: one consistent, colorblind-safe, print-friendly style.

Every figure is saved as both PNG (for the README) and SVG (for scaling).

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import os
import matplotlib

matplotlib.use("Agg")  # headless / CI-safe
import matplotlib.pyplot as plt


# A small, colorblind-safe palette (Okabe-Ito subset).
INK = "#1b1b1f"
GRID = "#d9dce1"
BLUE = "#0072b2"
ORANGE = "#e69f00"
GREEN = "#009e73"
RED = "#d55e00"
PURPLE = "#cc79a7"
MUTED = "#6b7280"


def apply_style() -> None:
    plt.rcParams.update({
        "figure.dpi": 120,
        "savefig.dpi": 150,
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "axes.edgecolor": INK,
        "axes.labelcolor": INK,
        "axes.titlecolor": INK,
        "axes.titlesize": 13,
        "axes.titleweight": "bold",
        "axes.labelsize": 11,
        "axes.grid": True,
        "grid.color": GRID,
        "grid.linewidth": 0.8,
        "xtick.color": INK,
        "ytick.color": INK,
        "font.size": 10,
        "legend.frameon": False,
        "figure.autolayout": True,
    })


def save(fig, out_dir: str, name: str) -> list[str]:
    """Save ``fig`` as PNG + SVG into ``out_dir``; return the paths written."""
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for ext in ("png", "svg"):
        p = os.path.join(out_dir, f"{name}.{ext}")
        fig.savefig(p, bbox_inches="tight")
        paths.append(p)
    plt.close(fig)
    return paths
