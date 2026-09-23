# SkimGuard — 60-Second Live Demo Guide

**Print this. Keep it face-down on the table.**

Project: SkimGuard — a passive 13.56 MHz NFC reader detector for Flipper Zero.
Author / sole contributor: Krishita Sanjay Choksi.
Demo promise: *the visitor finds the hidden reader themselves, in under a
minute, using a tool that never transmits.*

---

## Table setup (do once, before you run the demo)

| Item | Detail |
|---|---|
| Box A — "clean" | Empty. Nothing inside. Looks identical to B. |
| Box B — "compromised" | Powered 13.56 MHz NFC reader taped inside, off-center, low. |
| Flipper Zero | SkimGuard loaded, volume ON, battery > 50%. |
| Power | USB battery pack for the reader, hidden or run behind the table. |
| Backup | Second charged Flipper, or the `SKIMGUARD_SIM` build / simulator replay on a laptop. |

**Placement rules**

- Boxes at least **18 in / 45 cm apart** so Box B's field doesn't bleed into the
  sweep of Box A.
- Reader **off-center inside B** — not dead center, not at the lid. The hunt is
  the fun part.
- Keep your **phone and any access badge off the table.** They are NFC devices
  and they will muddy a sweep.
- Swap which box is "the bad one" every so often, so someone who watched twice
  can't tip off the next person.

**30-second pre-flight, before each session and after every break**

1. Power the reader in Box B. 2. Sweep Box A — must stay quiet. 3. Sweep Box B —
must light up and click faster as you close in. 4. Press OK to reset SkimGuard to
the idle screen.

If step 2 is not quiet, move the boxes farther apart before running the demo.

---

## The 60 seconds

### 0:00 — 0:10 · The hook

> **"One of these two terminals has a card skimmer hidden inside it. Want to find it?"**

Say it while you pick up the Flipper. Don't explain anything yet. Let the
question do the work — everyone already knows what a skimmer is, so there's
nothing to teach here.

*If they hesitate:* "Takes ten seconds, I promise."

### 0:10 — 0:20 · The hand-off

Put the Flipper **in their hand**, screen facing them. Then, only then, one
sentence of instruction:

> **"It clicks faster the closer you get. Start with this one."** *(point at the clean box)*

Nothing else. Don't hover, don't narrate, don't point at the screen. The demo is
theirs now. Step half a pace back — it makes them take ownership of the device.

### 0:20 — 0:40 · The sweep

They pass it over **Box A.** It stays quiet. Give it a beat; the silence is doing
real work — it establishes the baseline.

> **"That one's clean. Now the other one."**

They move to **Box B.** The meter climbs, the clicks tighten up, and they'll
start hunting on their own — they always do. Let them. **Do not tell them where
the reader is.**

When they lock on:

> **"You just found it."**

*If they drift away from it:* "It's telling you you're getting colder." Never
take the Flipper back.

### 0:40 — 0:50 · The reveal

Open Box B. Show the reader taped inside.

> **"That's the skimmer. And here's the thing — it gave itself away on its own.
> Any powered NFC reader is constantly putting out a 13.56 megahertz field just
> sitting there, waiting for a card. It has to. SkimGuard only listens for that
> field. It never transmits anything."**

Land on **"never transmits."** That one phrase is what makes this a security tool
and not a party trick.

### 0:50 — 1:00 · The defensive punchline

> **"This is exactly how you'd sweep an ATM, a gas pump, or a point-of-sale
> terminal. Checking for rogue readers isn't a gadget trick — it's a
> physical-security control. It belongs on an audit checklist next to badge
> access and camera coverage. I built the detector, then I measured it: how far
> out it detects, how it does against benign NFC like a phone or a transit card,
> and exactly what it can't see."**

Stop talking. Let them ask the next question.

---

## The questions you will get, and your answers

**"Did you invent this?"**
> "No, and I'm upfront about that. The passive field-detection approach comes
> from an open-source project called Specter. This is my own independent
> implementation of it, and I credit them in the README. What I added on top is
> the measurement layer — detection range, false-positive rate, field strength
> versus distance, all charted — and the control-mapping framing. Reimplementing
> a known technique to learn it and then extending it is real work; claiming I
> invented it wouldn't be."

Say this *readily*, not defensively. Volunteering it is the point — someone who
knows Specter will trust you for the rest of the conversation.

**"So this finds any skimmer?"**
> "No, and that limit matters. It finds *powered 13.56 megahertz readers.* It
> won't see a 125 kHz low-frequency prox reader, it won't see a skimmer that's
> asleep or shielded, and it won't see a purely mechanical card-trapping device.
> A quiet reading means 'nothing emitting right now' — it is not a certificate
> that a terminal is clean."

**"Could this be used to attack something?"**
> "It can't transmit. There's no attack path in it — it's a receiver. That's a
> design choice, not an accident."

**"What did the evaluation actually show?"**
> Give them one real number — your measured reliable detection range from
> `docs/eval_results.md` — then offer the charts. One concrete number beats a
> paragraph.

---

## Reset between visitors (10 seconds)

1. Close Box B. 2. Confirm the reader is still powered. 3. Press OK on SkimGuard
to recalibrate to idle. 4. Move both boxes slightly — so the next person doesn't
get a hint from scuff marks or a shifted box.

## Controls cheat-sheet

| Button | Action |
|---|---|
| Left / Right | switch mode: Sweep · A/B · Watch |
| OK (short) | Sweep: recalibrate here · A/B: next step · Watch: arm/disarm |
| OK (long) | reset session (fresh baseline, clear peak-hold + waveform) |
| Up | cycle feedback: mute → snd → snd+led (sound / haptic / LED) |
| Down (short) | toggle SD session logging |
| Down (long) | cycle sensitivity: High → Med → Low |
| Back | exit |

---

## If it goes wrong

| Problem | Say this, do this |
|---|---|
| Clean box registers | "That's my own phone bleeding into it — good illustration of the false-positive problem, actually." Move the interfering device. Widen the box spacing. |
| Compromised box is quiet | Check the USB power first, in front of them. "Reader lost power — which is the honest limitation: an unpowered skimmer is invisible to this." Recover, don't apologize twice. |
| Flipper dies | Swap to the spare, or pivot to the laptop: "Here's the same detection running on recorded traces from the simulator." |
| A crowd forms | Hand the Flipper to the **next** person and let the first one narrate. Watching someone else teach it sells it harder than you can. |

---

## Rules for the table

- Sweep **only your own boxes.** Never sweep a visitor's wallet, badge, phone, or
  bag, even if they offer — and if they offer, that refusal is a good look:
  *"I only sweep equipment I own or am authorized to assess. That's the same rule
  that applies in a real audit."*
- Never say a real-world terminal is "clean" based on this tool.
- Don't overstate. The honest version of this project is the strong one.

---

## One-line pitch, if you only get one line

> **"A Flipper Zero app that finds hidden card skimmers by listening for the RF
> field a powered reader can't help but emit — it never transmits, and I measured
> exactly how well it works and where it fails."**
