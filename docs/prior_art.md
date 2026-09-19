# Prior Art & Honest Positioning

**Short version:** SkimGuard does not invent the technique it uses. Passively
detecting a powered 13.56 MHz reader by sensing the RF field it emits is an
established idea, most visibly implemented by the open-source **Specter**
project for the Flipper Zero. SkimGuard is an **independent implementation** of
that approach, and this document exists so nobody has to guess where the line
is between "borrowed idea" and "own work."

## What is prior art here

The core principle — *a reader that is powered on is continuously emitting a
13.56 MHz field, and a receive-only device can sense that field to locate the
reader without transmitting* — predates this project and is not original to it.
The Specter project demonstrated this on Flipper Zero hardware and got
deserved attention for it. Credit for the technique and for showing it works on
this hardware belongs to that prior work.

If you are evaluating this repository and you know Specter, you should read the
similarity as *intentional and disclosed*, not as an undisclosed copy.

## What is SkimGuard's own work

SkimGuard is a clean-room, independent implementation with additions that the
reference approach does not focus on:

1. **A measurement / evaluation layer.** Most reader-detector apps show a
   needle and stop there. SkimGuard quantifies itself: detection range,
   false-positive rate against benign NFC devices (transit cards, blank tags,
   phones), and a field-strength-versus-distance curve — all code-generated and
   reproducible from a clean clone (`eval/`, `figures/`, `docs/eval_results.md`).
2. **A hardware-free simulator.** `sim/` models the reader field with a
   physically-motivated on-axis loop expression so the detection logic, the UI
   feedback curves, and the entire evaluation run and reproduce **without a
   Flipper**. The same CSV schema loads real captured logs, so one detector
   runs on both.
3. **A guided A/B booth mode and a watch/alert mode** built into the app.
4. **A physical-security-control framing.** SkimGuard is presented as a
   physical-security control (rogue-reader detection), mapped to recognized
   control families — see `docs/threat_model.md`.

## What SkimGuard does *not* claim

- It does not claim to have invented passive reader-field detection.
- It does not claim novelty for the on-device sensing approach.
- It does not claim to identify the exact make/model of a detected reader — it
  confirms a live reader is present and roughly how near, and characterizes the
  field as steady vs intermittent. Nothing finer than that.

## How to describe this project accurately

> "An independent implementation of the passive 13.56 MHz reader-field-detection
> approach (as popularized by the open-source Specter project), with an added
> measurement/evaluation layer and a physical-security-control framing."

Reimplementing a known technique to understand it and then extending it is
legitimate engineering work. Overstating novelty would not be, so this project
does not.

## References to consult

- The Specter Flipper Zero project (the reference implementation to study and
  credit).
- Flipper Zero NFC HAL (`furi_hal_nfc`) and the ST25R3916 front-end amplitude
  measurement — the mechanism the field sensor relies on.
- General NFC/RFID skimmer-detection literature and the distinction between
  13.56 MHz (in scope) and 125 kHz low-frequency prox readers (out of scope).
