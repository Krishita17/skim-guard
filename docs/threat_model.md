# Threat Model & Scope

## Purpose

SkimGuard is a **defensive counter-surveillance tool**: it helps a person find a
hidden or rogue 13.56 MHz NFC reader — for example a skimmer concealed in a
payment terminal, ATM fascia, or kiosk — by passively sensing the RF field the
reader emits. It is a bug-sweep for card readers.

**It only listens. It never transmits.** There is no polling, no field
energizing, no emulation, no attack path anywhere in the code. It is a receiver.

## Who it is for

- People sweeping terminals, kiosks, or spaces **they own or are authorized to
  assess** (facilities staff, a shop owner checking their own POS, a security
  team doing a physical audit, a curious owner checking their own hardware).

## What it detects

- **Powered 13.56 MHz NFC readers** whose field is currently on. This includes
  many skimmers, because a skimmer that wants to read a card must energize a
  field to do so.
- It reports **present / absent**, an estimated **proximity** (how near), and a
  coarse **steady vs intermittent** characterization of the field.

## What it does NOT detect (state this plainly)

- **125 kHz low-frequency prox readers** (older badge/prox systems). These do
  not present the same 13.56 MHz field-detect signal. Out of scope.
- **A reader that is powered off, asleep, or duty-cycled off at the moment of
  the sweep.** If it is not emitting, there is nothing to sense.
- **Shielded readers** whose field is deliberately contained.
- **Purely mechanical attacks** (card-trapping "Lebanese loops", skimmer
  overlays that store to internal memory without an active RF read, hidden
  cameras, PIN-pad overlays). SkimGuard senses RF fields, not physical tampering.
- **The exact device.** A positive reading means "a live reader is here and
  roughly this near," not "this is a Model X skimmer."

### The most important limitation

> A quiet reading means **"nothing is emitting a 13.56 MHz field right now."**
> It is **not** a certificate that a terminal is clean.

Absence of a detection is not proof of absence of a skimmer. Any writeup, demo,
or report using this tool must say so.

## Non-goals

- No transmit, replay, emulation, cloning, or reader-attack capability.
- No identification, fingerprinting, or profiling of nearby cards/phones.
- No data exfiltration; the tool reads a field strength, nothing more.

## Responsible use

- Sweep only equipment and spaces you own or are explicitly authorized to
  assess. Do not sweep other people's cards, badges, wallets, or devices.
- Do not represent a "clean" reading as a guarantee that a real-world terminal
  is safe.
- Local laws on RF monitoring and on inspecting payment infrastructure vary;
  follow them.

## Relation to physical-security controls

Detecting rogue/unauthorized readers is a **physical-security control**, not a
gadget trick. It maps naturally onto recognized control families:

- **NIST SP 800-53 — PE (Physical & Environmental Protection):** monitoring for
  unauthorized devices attached to or near assets; physical inspection of
  facilities and equipment (e.g. PE-3, PE-6, PE-20 themes).
- **NIST SP 800-53 — SC / SI:** detection of unauthorized transmitting/receiving
  equipment as part of environment monitoring.
- **PCI DSS Requirement 9 (notably 9.5.x):** periodic inspection of POS devices
  and payment terminals for tampering and substitution — a rogue-reader sweep is
  a concrete way to operationalize that inspection.

See `docs/prior_art.md` for how the technique is credited, and
`docs/eval_results.md` for the measured limits behind these claims.
