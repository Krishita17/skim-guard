"""Reference implementation of the SkimGuard detection pipeline.

This is a plain-Python mirror of the algorithm that runs on the Flipper in
``src/detection.c`` / ``src/signal.c``. Keeping a reference copy here lets the
evaluation score *exactly the logic the firmware uses* against thousands of
simulated and recorded samples — something you cannot do on-device.

The two implementations are intentionally kept in lock-step; the shared
constants live in :data:`PARAMS` and are duplicated as ``#define``s in
``src/skimguard_config.h``. If you change one, change the other.

Pipeline (per sample):
    1. Calibrate a baseline + noise estimate from the first CALIB_SAMPLES.
    2. Exponentially smooth the raw reading (fast EMA).
    3. signal = smoothed - baseline.
    4. Latch present/absent with hysteresis, expressed in multiples of the
       measured noise standard deviation (so it self-tunes to the environment).
    5. Estimate proximity 0..100% from signal strength.
    6. Estimate a warmer/colder trend from a slow EMA.
    7. Map proximity to a geiger click interval.
    8. Characterize steady vs intermittent over a sliding window.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List
import numpy as np


# --- Shared tuning constants (mirror src/skimguard_config.h) -----------------
PARAMS = dict(
    CALIB_SAMPLES=20,        # samples used to learn the baseline at startup
    EMA_ALPHA_FAST=0.35,     # smoothing for the live reading
    EMA_ALPHA_SLOW=0.08,     # slow reference used for the warmer/colder trend
    NOISE_MIN=4.0,           # floor on the noise estimate (ADC counts)
    ON_SIGMA=6.0,            # signal must exceed ON_SIGMA * noise to latch ON
    OFF_SIGMA=3.5,           # drops OFF below OFF_SIGMA * noise (hysteresis)
    ON_MIN_COUNTS=25.0,      # absolute floor for the ON threshold
    PROX_REF_COUNTS=650.0,   # signal mapped to 100% proximity
    CLICK_MS_FAR=900.0,      # click interval when barely present
    CLICK_MS_NEAR=45.0,      # click interval when locked on
    TREND_EPS=6.0,           # min delta (counts) to call a trend warmer/colder
    WINDOW_SAMPLES=40,       # sliding window for steady/intermittent call
    INTERMITTENT_LO=0.20,    # duty in [LO, HI] within window => intermittent
    INTERMITTENT_HI=0.85,
)


@dataclass
class DetectorState:
    """Per-sample output of the detector."""

    reading: float
    smoothed: float
    slow: float
    baseline: float
    noise: float
    signal: float
    present: bool
    proximity_pct: float
    trend: int            # +1 warmer, -1 colder, 0 steady
    click_ms: float
    characterization: str  # 'none' | 'steady' | 'intermittent'


class Detector:
    """Streaming detector. Feed it one raw reading at a time via :meth:`push`."""

    def __init__(self, params: dict | None = None):
        p = dict(PARAMS)
        if params:
            p.update(params)
        self.p = p

        self._calib_buf: List[float] = []
        self.baseline = 0.0
        self.noise = p["NOISE_MIN"]
        self.calibrated = False

        self.smoothed = 0.0
        self.slow = 0.0
        self.present = False
        self._present_hist: List[bool] = []

    # -- calibration ----------------------------------------------------------
    def _calibrate(self, reading: float) -> None:
        self._calib_buf.append(reading)
        if len(self._calib_buf) >= self.p["CALIB_SAMPLES"]:
            arr = np.asarray(self._calib_buf, dtype=float)
            self.baseline = float(np.mean(arr))
            self.noise = float(max(np.std(arr), self.p["NOISE_MIN"]))
            self.smoothed = self.baseline
            self.slow = self.baseline
            self.calibrated = True

    def force_calibration(self, baseline: float, noise: float) -> None:
        """Skip the warm-up (used when a known-clean baseline is supplied)."""
        self.baseline = baseline
        self.noise = max(noise, self.p["NOISE_MIN"])
        self.smoothed = baseline
        self.slow = baseline
        self.calibrated = True

    # -- thresholds -----------------------------------------------------------
    @property
    def on_threshold(self) -> float:
        return max(self.p["ON_SIGMA"] * self.noise, self.p["ON_MIN_COUNTS"])

    @property
    def off_threshold(self) -> float:
        return self.p["OFF_SIGMA"] * self.noise

    # -- main step ------------------------------------------------------------
    def push(self, reading: float) -> DetectorState:
        p = self.p
        reading = float(reading)

        if not self.calibrated:
            self._calibrate(reading)
            # During calibration we report a benign, absent state.
            return DetectorState(
                reading=reading, smoothed=reading, slow=reading,
                baseline=self.baseline, noise=self.noise, signal=0.0,
                present=False, proximity_pct=0.0, trend=0,
                click_ms=p["CLICK_MS_FAR"], characterization="none",
            )

        # 1. smoothing
        af, as_ = p["EMA_ALPHA_FAST"], p["EMA_ALPHA_SLOW"]
        prev_slow = self.slow
        self.smoothed = af * reading + (1 - af) * self.smoothed
        self.slow = as_ * reading + (1 - as_) * self.slow

        # 2. signal above baseline
        signal = max(0.0, self.smoothed - self.baseline)

        # 3. hysteresis latch
        if self.present:
            if signal < self.off_threshold:
                self.present = False
        else:
            if signal > self.on_threshold:
                self.present = True

        # 4. proximity
        prox = 100.0 * signal / p["PROX_REF_COUNTS"]
        prox = float(np.clip(prox, 0.0, 100.0))
        if not self.present:
            prox = 0.0

        # 5. trend from slow EMA delta
        delta = self.slow - prev_slow
        if delta > p["TREND_EPS"]:
            trend = 1
        elif delta < -p["TREND_EPS"]:
            trend = -1
        else:
            trend = 0

        # 6. click cadence: interpolate FAR..NEAR by proximity
        frac = prox / 100.0
        click_ms = p["CLICK_MS_FAR"] + (p["CLICK_MS_NEAR"] - p["CLICK_MS_FAR"]) * frac
        if not self.present:
            click_ms = float("inf")  # silent when nothing detected

        # 7. steady vs intermittent over the sliding window
        self._present_hist.append(self.present)
        if len(self._present_hist) > p["WINDOW_SAMPLES"]:
            self._present_hist.pop(0)
        characterization = self._characterize()

        return DetectorState(
            reading=reading, smoothed=self.smoothed, slow=self.slow,
            baseline=self.baseline, noise=self.noise, signal=signal,
            present=self.present, proximity_pct=prox, trend=trend,
            click_ms=click_ms, characterization=characterization,
        )

    def _characterize(self) -> str:
        hist = self._present_hist
        if len(hist) < self.p["WINDOW_SAMPLES"] // 2:
            return "none"
        duty = float(np.mean(hist))
        if duty <= 0.02:
            return "none"
        if self.p["INTERMITTENT_LO"] <= duty <= self.p["INTERMITTENT_HI"]:
            return "intermittent"
        if duty > self.p["INTERMITTENT_HI"]:
            return "steady"
        return "none"


def run_detector(readings, params: dict | None = None) -> List[DetectorState]:
    """Convenience: run a fresh detector over an iterable of readings."""
    det = Detector(params)
    return [det.push(r) for r in readings]
