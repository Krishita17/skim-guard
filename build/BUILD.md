# Building & installing SkimGuard

SkimGuard has two independent halves:

- **On-device app** (`src/*.c`, `application.fam`) — built with **ufbt** into a
  `.fap`, or dropped onto the SD card.
- **Off-device tooling** (`sim/`, `eval/`) — plain Python, no hardware needed.

---

## 1. On-device app (the `.fap`)

### Option A — build with ufbt (recommended)

[`ufbt`](https://pypi.org/project/ufbt/) is the micro Flipper build tool.

```bash
pip install ufbt        # one-time
cd skim-guard
ufbt                    # builds dist/skimguard.fap
ufbt launch             # build, upload to a connected Flipper, and run
```

The first `ufbt` call downloads the matching firmware SDK. `ufbt` reads
`application.fam` and compiles every file in `src/`.

### Option B — drag-and-drop with qFlipper

1. Build the `.fap` once (Option A) to get `dist/skimguard.fap`.
2. Open **qFlipper**, go to the SD card, and copy `skimguard.fap` into
   `SD Card/apps/NFC/`.
3. On the Flipper: **Apps → NFC → SkimGuard**.

### Hardware-free / simulator build

To build the app with the built-in synthetic sensor (useful as a demo backup or
for a Flipper with no reader nearby), define `SKIMGUARD_SIM`:

```bash
ufbt FAP_CFLAGS="-DSKIMGUARD_SIM"
```

The app then shows a `SIM` marker and replays a synthetic approach sweep so the
meter and clicks move with no external reader present.

---

## Firmware compatibility — the one integration point

The only hardware-specific code is `hw_read_field_amplitude()` in
`src/field_sensor.c`. It must return an amplitude proportional to the external
13.56 MHz field, **passively** (measure only — never poll or energize).

The exact `furi_hal_nfc` entry point that exposes the ST25R3916 field
amplitude/field-present state has changed across firmware releases and community
forks. When porting:

- If your firmware exposes a **field-present boolean**, the provided fallback
  works but proximity is coarse (near/far).
- If it exposes a **raw amplitude/RSSI register**, map it in
  `hw_read_field_amplitude()` for a graduated reading — this is what makes the
  meter smooth and the range measurable.

Nothing in `field_sensor.c` calls a transmit/poll/emulate API, by design. Keep
it that way when porting: SkimGuard is receive-only.

---

## 2. Off-device tooling (simulator + evaluation)

No Flipper required.

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

make samples     # regenerate data/synthetic/*.csv
make figures     # run the eval study + build every figure in figures/
make test        # C unit tests (needs a C compiler) + Python tests
```

Or directly:

```bash
python -m sim.generate_samples
python -m eval.run_eval --seed 7
python -m eval.make_diagrams
python -m sim approach --power 1.0 --seed 3      # print one trace as CSV
```

---

## 3. Host unit tests for the firmware logic

The detection algorithm compiles and runs on your computer (no ufbt):

```bash
cc -std=c11 -Wall -Wextra -DSKIMGUARD_SIM -Isrc -o sg_test tests/test_detector.c -lm
./sg_test
```

This is what CI runs, so a logic regression fails the build even though CI has
no Flipper.
