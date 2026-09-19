"""Read/write SkimGuard traces in a single shared CSV schema.

Every trace — synthetic (from ``sim/``) or real (captured on a Flipper and
exported to ``data/real_logs/``) — uses the same columns, so the same detector
and the same plots run on both:

    t_s,distance_cm,reading,present

* ``t_s``        seconds since the sweep started (float)
* ``distance_cm``true distance to the reader, blank if unknown/absent
* ``reading``    raw sensor reading, ADC counts 0..1023 (int)
* ``present``    ground-truth: 1 if a reader field is emitting, else 0

For real captures, ``distance_cm`` and ``present`` are whatever the person
recorded by hand; leave them blank if they were not measured.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import csv
import os
from typing import List
import numpy as np

from sim.trace_generator import Trace

HEADER = ["t_s", "distance_cm", "reading", "present"]


def write_trace(path: str, trace: Trace) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(HEADER)
        for row in trace.to_rows():
            w.writerow(row)


def read_trace(path: str, kind: str = "loaded") -> Trace:
    t, dist, reading, label = [], [], [], []
    with open(path, newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            t.append(float(row["t_s"]))
            d = row.get("distance_cm", "")
            dist.append(float(d) if d not in ("", None) else np.inf)
            reading.append(float(row["reading"]))
            p = row.get("present", "")
            label.append(bool(int(p)) if p not in ("", None) else False)
    return Trace(
        t=np.asarray(t),
        distance_cm=np.asarray(dist),
        reading=np.asarray(reading),
        label=np.asarray(label, dtype=bool),
        kind=kind,
        meta={"source": path},
    )


def list_csv(directory: str) -> List[str]:
    if not os.path.isdir(directory):
        return []
    return sorted(
        os.path.join(directory, f)
        for f in os.listdir(directory)
        if f.endswith(".csv")
    )
