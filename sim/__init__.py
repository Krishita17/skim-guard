"""SkimGuard signal simulator package.

Generates synthetic 13.56 MHz reader field-strength traces so the detection
logic, the UI feedback curves, and the evaluation pipeline can be developed and
demonstrated without any Flipper Zero hardware.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from .field_model import FieldModel, FieldParams
from .trace_generator import (
    Trace,
    approach_trace,
    absent_trace,
    intermittent_trace,
    static_distance_trace,
)

__all__ = [
    "FieldModel",
    "FieldParams",
    "Trace",
    "approach_trace",
    "absent_trace",
    "intermittent_trace",
    "static_distance_trace",
]
