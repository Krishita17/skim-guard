# Real capture logs

This folder holds **de-identified logs the author captured on real hardware**,
plus one **illustrative example** showing the exact format so you can drop your
own captures in and have every tool work on them unchanged.

## Schema (identical to synthetic traces)

```
t_s,distance_cm,reading,present
```

| Column | Meaning |
|---|---|
| `t_s` | seconds since the sweep started (float) |
| `distance_cm` | hand-measured distance to the reader; leave blank if unknown |
| `reading` | raw sensor reading, ADC counts 0..1023 (int) |
| `present` | ground truth you recorded: `1` reader emitting, `0` not; blank if unlabeled |

Because the schema matches `sim/`, the same reference detector and the same
plots run on real and synthetic traces:

```bash
python -c "from eval.traceio import read_trace; from eval.detector import run_detector; \
tr=read_trace('data/real_logs/example_session.csv'); \
print('present samples:', sum(s.present for s in run_detector(tr.reading)))"
```

## `example_session.csv`

`example_session.csv` is an **illustrative example in the capture format** — it
is *not* a claim of a specific hardware measurement. Replace it with your own
real captures. To keep the repo honest and small:

- Commit only **small, de-identified** logs.
- **Never** commit anything tied to a terminal, ATM, POS device, or system you
  do not own or are not authorized to assess.
- Strip anything that could identify a location or a third party.

## How the author captured real logs

1. Build with the hardware back end and flash the `.fap` (see `build/BUILD.md`).
2. Add a stream-to-log path in `field_sensor.c` (or read the on-screen values)
   and record `reading` at the sample rate while sweeping a **reader you own**
   at known, hand-measured distances.
3. Note `present`/`distance_cm` by hand.
4. Save as CSV with the header above and drop it here.
