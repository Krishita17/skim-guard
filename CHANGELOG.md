# Changelog

All notable changes to SkimGuard. Sole author: Krishita Sanjay Choksi.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/).

## [1.2.0] — 2026-09-23

Analytical features unique to SkimGuard, implemented in the shared detection
engine (firmware `src/detection.c` + Python reference `eval/detector.py` in
lock-step) and covered by tests on both sides.

### Added
- **Detection-confidence score** — SNR-based (signal in noise-sigmas, scaled
  0–100%) plus a rolled-up **room verdict**: `CLEAR / TRACE / ACTIVE`.
- **Approximate distance estimate (cm)** — inverts the on-axis-loop + saturation
  model against a nominal reference reader; honestly flagged *uncalibrated*.
  New eval study/figure `distance_estimation` (mean abs. error ≈ 0.5 cm under
  reader-to-reader variation).
- **Poll-rate estimation (Hz)** for intermittent emitters, from rising-edge
  spacing of the presence signal.
- **Baseline drift auto-compensation** — slowly re-learns the RF floor while
  clear (after ~3 s) so environmental drift doesn't become a false positive.
- **Sensitivity presets** — High / Med / Low (long-press **Down**), scaling the
  noise-relative thresholds.
- On-device readouts for confidence (`cNN%`), distance (`~Ncm`), poll rate
  (`X.XHz`), verdict, and sensitivity; UI mockups regenerated.

### Testing
- +5 host C tests and +5 Python tests for the new features (confidence, distance
  monotonicity + accuracy-vs-truth, sensitivity thresholds, drift suppression,
  poll-rate estimate). Totals: **324 C checks, 16 Python tests**.

## [1.1.0] — 2026-09-19

Feature-parity pass with the reference project ([Specter](https://github.com/at0m-b0mb/Specter-FlipperZero)
by at0m-b0mb) plus SkimGuard's own additions.

### Added
- **Live waveform** strip on the Sweep screen (last ~3 s of proximity).
- **Peak-hold** marker on the meter and a numeric `pk%` readout.
- **Three-level feedback**, cycled with **Up**: `mute` → `snd` → `snd+led`.
  - **Haptic vibration** pulses synced to the geiger clicks.
  - **Proportional LED**: green when clear, red brightening toward the reader.
- **SD-card session logging** (toggle with **Down**): writes
  `/ext/apps_data/skimguard/session_<ts>.csv` in the **same CSV schema the
  offline evaluation uses**, so on-device captures load straight into `sim/`+
  `eval/` (`src/logger.c`).
- **OK (long press)** resets the session: fresh baseline, cleared peak-hold and
  waveform.

### Changed
- Input scheme: **Up** now cycles feedback level and **Down** toggles logging
  (previously Up/Down toggled sound).
- Sweep screen redesigned to fit the waveform, meter, peak, and a status/flags
  line; UI-state mockups in `figures/` regenerated to match.

### Notes
- The waveform, peak-hold, and haptic/LED feedback reach **parity** with
  Specter's on-device experience; they are independently implemented and
  credited. SkimGuard's distinct pieces remain the **guided A/B mode**, the
  **schema-matched SD logs**, the **reproducible measurement study**, the
  **hardware-free simulator + firmware-mirroring detector + tests/CI**, and the
  **physical-security-control framing**.

## [1.0.0] — 2026-09-19

### Added
- Initial release: passive 13.56 MHz reader detector for Flipper Zero
  (receive-only), EMF meter, accelerating geiger clicks, present/absent +
  proximity, steady-vs-intermittent characterization.
- **Sweep**, **A/B (clean vs compromised)**, and **Watch** modes.
- Hardware-free **simulator** (`sim/`), **reference detector** mirroring the
  firmware, and a **measurement study** (`eval/`) generating all figures.
- Host **C unit tests** + **Python tests** + **CI**.
- Docs: threat model, prior-art positioning, and the 60-second demo guide.
