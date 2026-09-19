"""Regenerate the committed synthetic sample traces in ``data/synthetic/``.

Deterministic (fixed seed) so the repository's sample data is reproducible:

    python -m sim.generate_samples

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import os
import numpy as np

from sim.field_model import FieldModel, FieldParams
from sim import trace_generator as tg
from eval.traceio import write_trace

OUT_DIR = os.path.join("data", "synthetic")
SEED = 42


def main():
    rng = np.random.default_rng(SEED)
    reader = FieldParams(power=1.0, coil_radius_cm=3.0, noise_floor=18.0, noise_sigma=6.0)
    weak = FieldParams(power=0.4, coil_radius_cm=2.5, noise_floor=18.0, noise_sigma=6.0)
    strong = FieldParams(power=2.0, coil_radius_cm=4.0, noise_floor=18.0, noise_sigma=6.0)

    samples = {
        "approach_typical.csv": tg.approach_trace(FieldModel(reader, rng), start_cm=25, closest_cm=1.5),
        "approach_weak_reader.csv": tg.approach_trace(FieldModel(weak, rng), start_cm=15, closest_cm=1.0),
        "approach_strong_reader.csv": tg.approach_trace(FieldModel(strong, rng), start_cm=40, closest_cm=2.0),
        "absent_clean.csv": tg.absent_trace(FieldModel(reader, rng), duration_s=6.0),
        "intermittent_poller.csv": tg.intermittent_trace(FieldModel(reader, rng), distance_cm=6.0),
        "static_10cm.csv": tg.static_distance_trace(FieldModel(reader, rng), 10.0, duration_s=3.0),
        "benign_transit_card.csv": tg.benign_device_trace("transit_card", rng=rng),
        "benign_phone_hce.csv": tg.benign_device_trace("phone_hce", rng=rng),
    }

    os.makedirs(OUT_DIR, exist_ok=True)
    for name, trace in samples.items():
        path = os.path.join(OUT_DIR, name)
        write_trace(path, trace)
        print(f"  wrote {path}  ({len(trace.reading)} samples, kind={trace.kind})")

    print(f"[sim] {len(samples)} sample traces written to {OUT_DIR} (seed={SEED})")


if __name__ == "__main__":
    main()
