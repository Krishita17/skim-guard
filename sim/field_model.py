"""Physical-ish model of what SkimGuard's passive field sensor reports.

A powered 13.56 MHz reader drives a loop antenna that radiates a near-field
magnetic field. Close to the coil the field is dominated by the classic on-axis
loop expression, which falls off steeply with distance:

    H(d)  proportional to  a^2 / (a^2 + d^2)^(3/2)

where ``a`` is the effective coil radius and ``d`` is the axial distance. The
Flipper's field-detect front end turns that field into a bounded ADC-style
reading, so we pass ``H`` through a saturating response (the reading flattens
out when the coils are almost touching) and add an electronic noise floor plus
Gaussian measurement noise and quantization.

None of this claims to be a calibrated EM solver. It is a *plausible,
monotonic, tunable* stand-in whose only job is to let the detection logic, the
UI feedback curves, and the evaluation run without hardware — and to let anyone
who clones the repo reproduce every figure. Real captured traces (see
``data/real_logs/``) use the exact same CSV schema, so the same detection code
runs on both.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

from dataclasses import dataclass
import numpy as np


# The sensor's full-scale reading, in arbitrary ADC counts. Kept as a module
# constant so the firmware, the simulator, and the eval all agree on the range.
FULL_SCALE = 1023.0


@dataclass
class FieldParams:
    """Tunable knobs for one reader / environment.

    Attributes:
        power: Relative drive strength of the reader (1.0 = a typical desktop
            USB NFC reader). A weak reader might be 0.4; a beefy payment
            terminal 2.0.
        coil_radius_cm: Effective antenna radius. Controls how quickly the
            field collapses with distance.
        half_saturation: Field value at which the sensor reads half of
            full-scale. Larger = the reading saturates less easily.
        noise_floor: Mean electronic/ambient reading with no field present.
        noise_sigma: Standard deviation of per-sample Gaussian noise.
        quantize: If True, round readings to integer ADC counts.
    """

    power: float = 1.0
    coil_radius_cm: float = 3.0
    half_saturation: float = 0.15
    noise_floor: float = 18.0
    noise_sigma: float = 6.0
    quantize: bool = True


class FieldModel:
    """Maps distance (cm) to a simulated sensor reading (ADC counts)."""

    def __init__(self, params: FieldParams | None = None, rng: np.random.Generator | None = None):
        self.params = params or FieldParams()
        self.rng = rng or np.random.default_rng()

    def field_strength(self, distance_cm: np.ndarray | float) -> np.ndarray:
        """Noise-free normalized field H(d) for the on-axis loop model."""
        d = np.asarray(distance_cm, dtype=float)
        a = self.params.coil_radius_cm
        h = self.params.power * (a ** 2) / np.power(a ** 2 + d ** 2, 1.5)
        return h

    def _saturate(self, h: np.ndarray) -> np.ndarray:
        """Bounded sensor response: 0 at H=0, -> FULL_SCALE as H -> inf."""
        k = self.params.half_saturation
        return FULL_SCALE * (h / (h + k))

    def reading(
        self,
        distance_cm: np.ndarray | float,
        active: bool = True,
    ) -> np.ndarray:
        """Simulated raw sensor reading in ADC counts.

        Args:
            distance_cm: Distance(s) from the reader coil.
            active: If False, the reader is off — only the noise floor is seen.
        """
        d = np.atleast_1d(np.asarray(distance_cm, dtype=float))
        p = self.params

        if active:
            signal = self._saturate(self.field_strength(d))
        else:
            signal = np.zeros_like(d)

        reading = p.noise_floor + signal
        reading = reading + self.rng.normal(0.0, p.noise_sigma, size=d.shape)
        reading = np.clip(reading, 0.0, FULL_SCALE)

        if p.quantize:
            reading = np.round(reading)

        return reading if reading.size > 1 else reading[0]
