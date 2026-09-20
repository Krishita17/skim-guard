# SkimGuard

**A passive 13.56 MHz NFC reader (skimmer) detector for the Flipper Zero — it
listens for the RF field a powered reader can't help but emit, and never
transmits anything itself.**

> One-line pitch: wave the Flipper near a hidden card reader and SkimGuard turns
> the reader's own RF emissions into a find-it-yourself bug sweep — an
> accelerating "geiger" click and a rising meter as you close in — plus a
> measured evaluation of how far it reaches and where it fails.

**Author / sole contributor:** Krishita Sanjay Choksi. This project is authored
and maintained by Krishita Sanjay Choksi as the **sole contributor**; there are
no other contributors.

---

## Honest positioning (read this first)

SkimGuard does **not** invent the technique it uses. Passively detecting a
powered 13.56 MHz reader by sensing its field is an established idea, most
visibly implemented by the open-source **[Specter](https://github.com/at0m-b0mb/Specter-FlipperZero)**
project for the Flipper Zero, created by **at0m-b0mb (@at0m-b0mb)** and released
under the MIT license — tagline *"Sweep for the readers you can't see."* Full
credit for the passive reader-field-detection approach on this hardware belongs
to that project. SkimGuard is an **independent implementation** of that
approach, with two things added on top:

1. a **measurement / evaluation layer** (detection range, false-positive rate,
   field-strength-vs-distance — all code-generated and reproducible), and
2. a **physical-security-control framing** (rogue-reader detection mapped to
   recognized controls).

It does not claim novelty for the sensing approach, and it cannot identify the
exact device it detects. Full credit and scope: [`docs/prior_art.md`](docs/prior_art.md).

---

## Why it exists

Card skimmers hide inside ATMs, gas pumps, and payment terminals. A skimmer that
reads a card has to **power a 13.56 MHz field** to do it — and a powered reader
emits that field continuously, whether or not a card is present. SkimGuard is a
**receive-only** bug sweep: it senses that field and turns it into a live
warmer/colder meter so you can locate a hidden reader **without transmitting
anything yourself**. Detecting rogue readers is a real physical-security control
(see [Physical-security relevance](#physical-security-relevance)).

---

## Features

**On-device (`.fap`)**
- **Passive field sensing** at 13.56 MHz — receive-only, never transmits.
- **EMF-style meter** with a **peak-hold** tick marking the strongest reading.
- **Live waveform** strip showing the last few seconds of proximity.
- **Accelerating geiger clicks** — the click rate rises as you close in.
- **Three feedback levels** (cycle with Up): `mute` → `snd` → `snd+led`
  (sound **+ haptic vibration + proportional LED**: green = clear, red brightens
  toward the reader).
- **Warmer/colder trend** and **steady-vs-intermittent** field characterization.
- **Three modes** (Left/Right):
  - **Sweep** — free-hand hunt with waveform, meter, and clicks.
  - **A/B** — guided *clean box → suspect box* comparison for a decisive contrast.
  - **Watch** — armed sentry that alerts on wake-on-detection while unattended.
- **SD-card session logging** (toggle with Down) that writes the **exact CSV
  schema the offline evaluation uses**, so a capture on the device drops straight
  into `sim/`+`eval/` with no conversion.
- **Simulator build** (`-DSKIMGUARD_SIM`) for a hardware-free demo/backup.

**Off-device (this repo)**
- **Reproducible measurement study**: detection range, false-positive rate,
  field-vs-distance — all seeded and code-generated.
- **Hardware-free simulator** + a **Python reference detector that mirrors the
  firmware**, with **unit tests and CI**.

---

## What's new & how this differs from Specter

SkimGuard shares Specter's core idea, and by design some features reach *parity*
with it. Being explicit about which is which:

| Capability | Specter (at0m-b0mb) | SkimGuard | Note |
|---|---|---|---|
| Passive 13.56 MHz field detection | yes | yes | shared core technique — credited |
| EMF meter + accelerating clicks | yes | yes | parity, independently implemented |
| Peak-hold indicator | yes | yes | parity |
| Live waveform | yes | yes | parity |
| Haptic + LED + sound feedback | yes | yes | parity (3-level cycle) |
| Watch / wake-on-detection mode | yes | yes | parity |
| Steady vs intermittent characterization | yes (fingerprint) | yes (lighter) | parity-ish |
| **Guided A/B (clean vs compromised) mode** | — | yes | **SkimGuard's own** |
| **SD logs in the offline-eval CSV schema** | CSV export | yes, schema-matched | **feeds the study directly** |
| **Reproducible measurement study + charts** | — | yes | **SkimGuard's differentiator** |
| **Simulator + firmware-mirroring detector + tests/CI** | — | yes | **SkimGuard's differentiator** |
| **Physical-security control mapping (NIST/PCI)** | — | yes | **SkimGuard's framing** |

**Changelog:** see [`CHANGELOG.md`](CHANGELOG.md). The honest one-line framing:
*an independent implementation of Specter's approach, brought to feature parity
on device and extended with an evaluation/reproducibility layer, a guided A/B
mode, and a compliance framing.* It does **not** claim novelty over the
detection technique — full positioning in [`docs/prior_art.md`](docs/prior_art.md).

---

## Architecture

```mermaid
flowchart LR
    RF["RF field from<br/>active reader<br/>(external)"] --> S["NFC field sensor<br/>furi_hal_nfc<br/>LISTEN ONLY"]
    S --> P["Signal processor<br/>EMA smoothing + trend"]
    P --> D["Detection logic<br/>present / proximity<br/>steady vs intermittent"]
    AB["Booth A/B mode<br/>clean vs compromised"] --> UI["UI + feedback<br/>EMF meter, geiger clicks<br/>verdict, watch/alert"]
    D --> UI
    P --> UI
    D --> L["Logger + eval<br/>range, FP rate<br/>field-vs-distance"]
    L --> OFF["Off-device: sim/ + eval/ + figures/<br/>(reproduces without hardware)"]

    classDef ext fill:#f2f4f7,stroke:#6b7280;
    classDef core fill:#e8f0fe,stroke:#1b1b1f;
    classDef ui fill:#fef3e6,stroke:#1b1b1f;
    classDef ev fill:#e6f6ef,stroke:#1b1b1f;
    class RF,OFF ext; class S,P,D core; class AB,UI ui; class L ev;
```

Rendered copy (code-generated by `python -m eval.make_diagrams`):

![Architecture](figures/architecture.png)

| Component | Role |
|---|---|
| **NFC field sensor** (`src/field_sensor.c`) | Passive read of the external field via `furi_hal_nfc`. Never energizes a field. Only hardware-specific file. |
| **Signal processor** (`src/detection.c`) | EMA smoothing + a slow-EMA trend for warmer/colder. |
| **Detection logic** (`src/detection.c`) | Present/absent with noise-relative hysteresis, proximity estimate, steady-vs-intermittent characterization. |
| **UI + feedback** (`src/skimguard.c`) | EMF meter, accelerating clicks, verdict, A/B mode, watch/alert mode. |
| **Simulator** (`sim/`) | Physically-motivated field model → traces, so everything runs with no Flipper. |
| **Evaluation** (`eval/`) | Runs the reference detector over traces and generates all figures. |

The reference detector in `eval/detector.py` is a line-for-line mirror of the
firmware in `src/detection.c` (shared constants in `src/skimguard_config.h`), so
the off-device evaluation characterizes exactly what runs on the device.

---

## The 60-second demo

Hide a powered NFC reader in one of two identical boxes; leave the other empty.
Hand someone the Flipper: they sweep the empty box (quiet), then the other one —
the clicks accelerate and the meter climbs until they find the reader
themselves. Then open the box: *"it gave itself away, because a powered reader is
always emitting — and this only listens."*

Full script, setup, controls, and failure recovery: [`docs/demo_guide.md`](docs/demo_guide.md).

### On-device UI states (mockups)

| Idle — clear | Locked on (Sweep) | Watch alert |
|---|---|---|
| ![idle](figures/ui_idle.png) | ![locked](figures/ui_locked.png) | ![watch alert](figures/ui_verdict.png) |

*(These are code-generated mockups of the 128×64 screen, not hardware photos.)*

### Controls

| Button | Action |
|---|---|
| Left / Right | switch mode: **Sweep · A/B · Watch** |
| OK (short) | Sweep: recalibrate here · A/B: next step · Watch: arm/disarm |
| OK (long) | reset session: fresh baseline, clear peak-hold + waveform |
| Up | cycle feedback: **mute → snd → snd+led** (sound/haptic/LED) |
| Down | toggle **SD session logging** |
| Back | exit |

---

## Install

### Build the `.fap` with ufbt

```bash
pip install ufbt
ufbt            # builds dist/skimguard.fap
ufbt launch     # build + upload to a connected Flipper + run
```

### Or drag-and-drop with qFlipper

Copy `dist/skimguard.fap` to `SD Card/apps/NFC/`, then open **Apps → NFC →
SkimGuard** on the device.

### Hardware-free demo build

```bash
ufbt FAP_CFLAGS="-DSKIMGUARD_SIM"   # replays a synthetic sweep; shows a SIM marker
```

Full build notes, including the single firmware integration point, are in
[`build/BUILD.md`](build/BUILD.md).

---

## Reproduce the evaluation (no hardware)

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

make figures     # runs the study + regenerates every figure below
make test        # C detection tests + Python tests
```

Everything is seeded, so a clean clone reproduces the exact numbers.

### Results (seed 7, typical desktop USB reader)

| Metric | Value |
|---|---|
| Reliable detection range (P ≥ 90%) | **12.0 cm** |
| Marginal range (P ≥ 50%) | 12.5 cm |
| True-positive rate (active reader @ 4 cm) | 100% |
| Overall false-positive rate (benign NFC) | 0.6% |
| Intermittent tracking precision / recall | 0.77 / 1.00 |

Full table and per-device breakdown: [`docs/eval_results.md`](docs/eval_results.md).

| | |
|---|---|
| ![field vs distance](figures/field_vs_distance.png) | ![detection range](figures/detection_range.png) |
| ![false positives](figures/false_positive.png) | ![clean vs compromised](figures/clean_vs_compromised.png) |

![intermittent tracking](figures/intermittent_tracking.png)

> The numbers above come from the simulator's field model — they characterize the
> **detector logic** end-to-end and reproduce anywhere. Swap in your own captured
> logs (`data/real_logs/`) to characterize your specific hardware; the same
> detector and plots run on both.

---

## Capabilities & limits

| SkimGuard **detects** | SkimGuard **does not** detect |
|---|---|
| Powered **13.56 MHz** NFC readers currently emitting | **125 kHz** low-frequency prox readers (out of scope) |
| Present / absent + estimated proximity | The **exact device** (make/model) |
| Steady vs intermittent field | A reader that is **off, asleep, or shielded** at sweep time |
| — passively, never transmitting | Purely **mechanical** attacks (card traps, overlays, cameras) |

> **A quiet reading means "nothing emitting a 13.56 MHz field right now." It is
> not a certificate that a terminal is clean.** Absence of a detection is not
> proof of absence of a skimmer. Full threat model: [`docs/threat_model.md`](docs/threat_model.md).

---

## Physical-security relevance

Sweeping for rogue/unauthorized readers is a **physical-security control**, not a
gadget trick. It maps onto recognized control families:

- **NIST SP 800-53 (PE — Physical & Environmental Protection):** monitoring for
  unauthorized devices near assets; physical inspection of equipment.
- **PCI DSS Requirement 9 (9.5.x):** periodic inspection of POS/payment devices
  for tampering and substitution — a reader sweep operationalizes that check.

This framing connects the tool to GRC/audit practice: the same reasoning that
puts badge access and camera coverage on an audit checklist puts rogue-reader
detection there too.

---

## Responsible use

SkimGuard is **defensive and receive-only** — no transmit, replay, emulation, or
attack path exists in the code. Use it only to sweep equipment and spaces you
**own or are authorized to assess**. Do not sweep other people's cards, badges,
or devices, and never present a quiet reading as a guarantee that a real-world
terminal is safe.

---

## Repository layout

```
skim-guard/
├── application.fam          Flipper app manifest
├── CHANGELOG.md             version history (what's new per release)
├── src/                     the .fap: field sensor, detection, UI, SD logger
├── sim/                     hardware-free trace simulator (+ CLI)
├── eval/                    reference detector, measurement studies, plots
├── data/                    synthetic traces + de-identified real-log format
├── figures/                 code-generated charts, architecture, UI mockups
├── tests/                   host C tests + Python tests
├── docs/                    threat_model.md · prior_art.md · demo_guide.md · eval_results.md
└── build/                   ufbt build notes + firmware integration point
```

---

## License

MIT © 2026 Krishita Sanjay Choksi. See [`LICENSE`](LICENSE). Citation metadata in
[`CITATION.cff`](CITATION.cff).
