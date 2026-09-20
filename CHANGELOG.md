# Changelog

All notable changes to SkimGuard. Sole author: Krishita Sanjay Choksi.

The format is loosely based on [Keep a Changelog](https://keepachangelog.com/).

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
