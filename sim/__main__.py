"""Command-line interface for the SkimGuard simulator.

Generate a single trace and print it as CSV to stdout (or a file):

    python -m sim approach --power 1.0 --seed 3
    python -m sim absent --duration 6 -o /tmp/absent.csv
    python -m sim intermittent --distance 6 --on 0.6 --off 0.9

Trace kinds: approach, static, absent, intermittent, benign.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import argparse
import csv
import sys
import numpy as np

from sim.field_model import FieldModel, FieldParams
from sim import trace_generator as tg
from eval.traceio import HEADER


def build(args) -> "tg.Trace":
    rng = np.random.default_rng(args.seed)
    params = FieldParams(power=args.power, coil_radius_cm=args.coil,
                         noise_floor=args.floor, noise_sigma=args.noise)
    model = FieldModel(params, rng=rng)

    if args.kind == "approach":
        return tg.approach_trace(model, start_cm=args.start, closest_cm=args.closest,
                                 duration_s=args.duration)
    if args.kind == "static":
        return tg.static_distance_trace(model, args.distance, duration_s=args.duration)
    if args.kind == "absent":
        return tg.absent_trace(model, duration_s=args.duration)
    if args.kind == "intermittent":
        return tg.intermittent_trace(model, distance_cm=args.distance,
                                     duration_s=args.duration, on_s=args.on, off_s=args.off)
    if args.kind == "benign":
        return tg.benign_device_trace(args.device, duration_s=args.duration, rng=rng)
    raise SystemExit(f"unknown kind: {args.kind}")


def main():
    ap = argparse.ArgumentParser(prog="python -m sim", description="Generate one SkimGuard trace.")
    ap.add_argument("kind", choices=["approach", "static", "absent", "intermittent", "benign"])
    ap.add_argument("-o", "--out", help="output CSV path (default: stdout)")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--duration", type=float, default=6.0)
    # field model
    ap.add_argument("--power", type=float, default=1.0)
    ap.add_argument("--coil", type=float, default=3.0)
    ap.add_argument("--floor", type=float, default=18.0)
    ap.add_argument("--noise", type=float, default=6.0)
    # kind-specific
    ap.add_argument("--start", type=float, default=25.0)
    ap.add_argument("--closest", type=float, default=1.5)
    ap.add_argument("--distance", type=float, default=6.0)
    ap.add_argument("--on", type=float, default=0.6)
    ap.add_argument("--off", type=float, default=0.9)
    ap.add_argument("--device", default="transit_card",
                    choices=["transit_card", "blank_tag", "phone_hce", "phone_idle"])
    args = ap.parse_args()

    trace = build(args)
    f = open(args.out, "w", newline="") if args.out else sys.stdout
    try:
        w = csv.writer(f)
        w.writerow(HEADER)
        for row in trace.to_rows():
            w.writerow(row)
    finally:
        if args.out:
            f.close()
            print(f"wrote {len(trace.reading)} samples to {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
