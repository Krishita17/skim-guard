"""Generate time-series field-strength traces from the :class:`FieldModel`.

A *trace* is a sequence of samples taken at a fixed rate, each with a timestamp,
the distance to the reader at that moment, and the simulated sensor reading.
Traces mimic the situations SkimGuard is used in:

* ``approach`` — the user sweeps toward a reader and back out again.
* ``static_distance`` — the Flipper is held at a fixed distance (used to
  characterize field-vs-distance and detection range).
* ``absent`` — no reader is powered anywhere nearby.
* ``intermittent`` — a reader that duty-cycles on and off (some skimmers poll).

Every trace carries a ``label`` (the ground truth) so the evaluation pipeline
can score the detector against it.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional
import numpy as np

from .field_model import FieldModel, FieldParams


DEFAULT_SAMPLE_HZ = 20.0  # matches the firmware's default poll cadence


@dataclass
class Trace:
    """One recorded sweep.

    Attributes:
        t: Timestamps in seconds.
        distance_cm: True distance to the reader at each sample
            (``inf`` when no reader is present).
        reading: Simulated raw sensor readings (ADC counts).
        label: Ground truth — True if a reader field is actually emitting at
            that sample, else False. Same length as ``reading``.
        kind: Human-readable trace type.
        meta: Free-form parameters used to generate the trace.
    """

    t: np.ndarray
    distance_cm: np.ndarray
    reading: np.ndarray
    label: np.ndarray
    kind: str
    meta: dict = field(default_factory=dict)

    def to_rows(self):
        """Yield CSV-ready rows: (t, distance_cm, reading, present)."""
        for ti, di, ri, li in zip(self.t, self.distance_cm, self.reading, self.label):
            yield (
                round(float(ti), 4),
                (round(float(di), 3) if np.isfinite(di) else ""),
                int(ri),
                int(bool(li)),
            )


def _time_axis(duration_s: float, sample_hz: float) -> np.ndarray:
    n = max(1, int(round(duration_s * sample_hz)))
    return np.arange(n) / sample_hz


def approach_trace(
    model: FieldModel,
    start_cm: float = 25.0,
    closest_cm: float = 1.0,
    duration_s: float = 6.0,
    sample_hz: float = DEFAULT_SAMPLE_HZ,
    dwell_frac: float = 0.15,
) -> Trace:
    """A sweep in toward the reader, a short dwell, then back out.

    ``dwell_frac`` is the fraction of time spent hovering near ``closest_cm``.
    """
    t = _time_axis(duration_s, sample_hz)
    n = len(t)
    phase = t / t[-1] if n > 1 else np.array([0.0])

    dwell = np.clip(dwell_frac, 0.0, 0.6)
    down = (1.0 - dwell) / 2.0
    up = 1.0 - down

    dist = np.empty(n)
    for i, p in enumerate(phase):
        if p < down:  # moving in
            frac = p / down
            dist[i] = start_cm + (closest_cm - start_cm) * frac
        elif p < up:  # dwelling near the reader
            dist[i] = closest_cm
        else:  # moving back out
            frac = (p - up) / max(1e-9, (1.0 - up))
            dist[i] = closest_cm + (start_cm - closest_cm) * frac

    reading = np.array([model.reading(d, active=True) for d in dist])
    label = np.ones(n, dtype=bool)
    return Trace(
        t=t,
        distance_cm=dist,
        reading=reading,
        label=label,
        kind="approach",
        meta=dict(start_cm=start_cm, closest_cm=closest_cm, duration_s=duration_s),
    )


def static_distance_trace(
    model: FieldModel,
    distance_cm: float,
    duration_s: float = 2.0,
    sample_hz: float = DEFAULT_SAMPLE_HZ,
) -> Trace:
    """The Flipper held still at a fixed distance from an active reader."""
    t = _time_axis(duration_s, sample_hz)
    n = len(t)
    dist = np.full(n, distance_cm)
    reading = model.reading(dist, active=True)
    reading = np.atleast_1d(reading)
    label = np.ones(n, dtype=bool)
    return Trace(
        t=t,
        distance_cm=dist,
        reading=reading,
        label=label,
        kind="static",
        meta=dict(distance_cm=distance_cm, duration_s=duration_s),
    )


def absent_trace(
    model: FieldModel,
    duration_s: float = 6.0,
    sample_hz: float = DEFAULT_SAMPLE_HZ,
) -> Trace:
    """No reader powered: only the noise floor. Ground-truth label all False."""
    t = _time_axis(duration_s, sample_hz)
    n = len(t)
    dist = np.full(n, np.inf)
    reading = model.reading(np.zeros(n), active=False)
    reading = np.atleast_1d(reading)
    label = np.zeros(n, dtype=bool)
    return Trace(
        t=t,
        distance_cm=dist,
        reading=reading,
        label=label,
        kind="absent",
        meta=dict(duration_s=duration_s),
    )


def intermittent_trace(
    model: FieldModel,
    distance_cm: float = 6.0,
    duration_s: float = 8.0,
    on_s: float = 0.6,
    off_s: float = 0.9,
    sample_hz: float = DEFAULT_SAMPLE_HZ,
) -> Trace:
    """A reader that duty-cycles on/off at a fixed distance.

    The ground-truth label follows the on/off pattern so the eval can score
    how well the detector tracks an intermittent emitter.
    """
    t = _time_axis(duration_s, sample_hz)
    n = len(t)
    period = on_s + off_s
    phase = np.mod(t, period)
    active_mask = phase < on_s

    dist = np.where(active_mask, distance_cm, np.inf)
    reading = np.empty(n)
    for i, on in enumerate(active_mask):
        d = distance_cm if on else 0.0
        reading[i] = model.reading(d, active=bool(on))

    return Trace(
        t=t,
        distance_cm=dist,
        reading=reading,
        label=active_mask.astype(bool),
        kind="intermittent",
        meta=dict(distance_cm=distance_cm, on_s=on_s, off_s=off_s, duration_s=duration_s),
    )


def benign_device_trace(
    kind: str,
    duration_s: float = 5.0,
    sample_hz: float = DEFAULT_SAMPLE_HZ,
    rng: Optional[np.random.Generator] = None,
) -> Trace:
    """A benign NFC *tag/card/phone* near the Flipper — NOT an active reader.

    This is the crux of the false-positive test. A passive tag (transit card,
    blank tag) does not emit a field of its own, so SkimGuard should read only
    the noise floor. A phone in card-emulation mode likewise does not drive a
    reader field. We model tiny occasional perturbations (the object physically
    detuning the antenna) that a naive threshold might trip on.

    ``kind`` is one of: 'transit_card', 'blank_tag', 'phone_hce', 'phone_idle'.
    Ground-truth label is all False (no reader field present).
    """
    rng = rng or np.random.default_rng()
    t = _time_axis(duration_s, sample_hz)
    n = len(t)

    # Base noise floor identical to the 'absent' case.
    base = FieldModel(FieldParams(), rng=rng)
    reading = np.array(base.reading(np.zeros(n), active=False), dtype=float)
    reading = np.atleast_1d(reading).astype(float)

    # Small, brief detuning bumps caused by the physical object — deliberately
    # well below a real reader's field but above the flat floor, to make the
    # false-positive test meaningful.
    bump = {
        "transit_card": 12.0,
        "blank_tag": 8.0,
        "phone_hce": 22.0,   # HCE phones can briefly perturb more
        "phone_idle": 6.0,
    }.get(kind, 8.0)

    n_bumps = rng.integers(1, 4)
    for _ in range(int(n_bumps)):
        center = rng.integers(0, n)
        width = max(1, int(0.15 * sample_hz))
        lo, hi = max(0, center - width), min(n, center + width)
        reading[lo:hi] += bump * rng.uniform(0.4, 1.0)

    reading = np.clip(reading, 0.0, 1023.0)
    dist = np.full(n, np.inf)
    label = np.zeros(n, dtype=bool)
    return Trace(
        t=t,
        distance_cm=dist,
        reading=np.round(reading),
        label=label,
        kind=f"benign:{kind}",
        meta=dict(device=kind, duration_s=duration_s),
    )
