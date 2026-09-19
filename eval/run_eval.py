"""SkimGuard measurement study — the differentiator layer.

Runs four studies against the reference detector and writes every figure and a
machine-readable results file. Everything is seeded, so a clean clone
reproduces the exact numbers and charts:

    python -m eval.run_eval --seed 7

Studies:
    1. Field-strength vs distance   -> figures/field_vs_distance.*
    2. Detection range (P detect)   -> figures/detection_range.*
    3. False positives vs benign    -> figures/false_positive.*
    4. Clean vs compromised (A/B)   -> figures/clean_vs_compromised.*
    (+ intermittent tracking)       -> figures/intermittent_tracking.*

Outputs: eval/results.json and docs/eval_results.md.

Author / sole contributor: Krishita Sanjay Choksi.
"""

from __future__ import annotations

import argparse
import json
import os
import numpy as np

from sim.field_model import FieldModel, FieldParams
from sim import trace_generator as tg
from eval.detector import Detector
from eval import plots


FIG_DIR = "figures"
RESULTS_JSON = os.path.join("eval", "results.json")
RESULTS_MD = os.path.join("docs", "eval_results.md")

# The "test reader" the whole study characterizes (a typical desktop USB reader).
TEST_READER = FieldParams(power=1.0, coil_radius_cm=3.0, noise_floor=18.0, noise_sigma=6.0)


def _clean_baseline(rng, n=400):
    """Estimate baseline + noise the way the firmware does at startup."""
    m = FieldModel(FieldParams(power=1.0, noise_floor=TEST_READER.noise_floor,
                               noise_sigma=TEST_READER.noise_sigma), rng=rng)
    absent = np.atleast_1d(m.reading(np.zeros(n), active=False)).astype(float)
    return float(absent.mean()), float(absent.std())


def _detected(readings, baseline, noise) -> bool:
    """True if the detector latches present at any point after warm-up."""
    det = Detector()
    det.force_calibration(baseline, noise)
    for r in readings:
        st = det.push(r)
        if st.present:
            return True
    return False


# --- Study 1: field strength vs distance -------------------------------------
def study_field_vs_distance(rng, baseline):
    distances = np.linspace(0.5, 30.0, 60)
    trials = 40
    model = FieldModel(TEST_READER, rng=rng)

    means, stds = [], []
    for d in distances:
        vals = []
        for _ in range(trials):
            r = float(np.atleast_1d(model.reading(d, active=True))[0])
            vals.append(max(0.0, r - baseline))
        means.append(np.mean(vals))
        stds.append(np.std(vals))
    means, stds = np.array(means), np.array(stds)

    fig, ax = plots.plt.subplots(figsize=(6.6, 4.2))
    ax.plot(distances, means, color=plots.BLUE, lw=2.2, label="mean signal above baseline")
    ax.fill_between(distances, means - stds, means + stds, color=plots.BLUE, alpha=0.15,
                    label="±1 s.d.")
    det = Detector(); det.force_calibration(baseline, TEST_READER.noise_sigma)
    ax.axhline(det.on_threshold, color=plots.RED, ls="--", lw=1.4,
               label=f"detection threshold ({det.on_threshold:.0f} counts)")
    ax.set_xlabel("Distance from reader (cm)")
    ax.set_ylabel("Sensor signal (ADC counts above baseline)")
    ax.set_title("Field strength vs distance — SkimGuard test reader")
    ax.legend()
    paths = plots.save(fig, FIG_DIR, "field_vs_distance")

    return dict(distances=distances.tolist(), mean_signal=means.tolist(),
                std_signal=stds.tolist(), figures=paths)


# --- Study 2: detection range ------------------------------------------------
def study_detection_range(rng, baseline, noise):
    distances = np.linspace(0.5, 22.0, 44)
    trials = 60
    model = FieldModel(TEST_READER, rng=rng)

    p_detect = []
    for d in distances:
        hits = 0
        for _ in range(trials):
            tr = tg.static_distance_trace(model, d, duration_s=1.5)
            if _detected(tr.reading, baseline, noise):
                hits += 1
        p_detect.append(hits / trials)
    p_detect = np.array(p_detect)

    # Reliable range = greatest distance at which P(detect) >= 0.90.
    reliable = 0.0
    for d, p in zip(distances, p_detect):
        if p >= 0.90:
            reliable = d
    # 50% range (usable-but-marginal).
    p50 = 0.0
    for d, p in zip(distances, p_detect):
        if p >= 0.50:
            p50 = d

    fig, ax = plots.plt.subplots(figsize=(6.6, 4.2))
    ax.plot(distances, p_detect * 100, color=plots.GREEN, lw=2.2, marker="o", ms=3)
    ax.axhline(90, color=plots.RED, ls="--", lw=1.2, label="90% reliability")
    ax.axvline(reliable, color=plots.MUTED, ls=":", lw=1.4,
               label=f"reliable range ≈ {reliable:.1f} cm")
    ax.set_xlabel("Distance from reader (cm)")
    ax.set_ylabel("Detection probability (%)")
    ax.set_title("Detection range — probability of latching on")
    ax.set_ylim(-3, 103)
    ax.legend()
    paths = plots.save(fig, FIG_DIR, "detection_range")

    return dict(distances=distances.tolist(), p_detect=p_detect.tolist(),
                reliable_range_cm=round(reliable, 2), range_50pct_cm=round(p50, 2),
                figures=paths)


# --- Study 3: false positives vs benign NFC ----------------------------------
def study_false_positives(rng, baseline, noise):
    benign = ["transit_card", "blank_tag", "phone_idle", "phone_hce"]
    trials = 80
    fp_rate = {}
    for dev in benign:
        flagged = 0
        for _ in range(trials):
            tr = tg.benign_device_trace(dev, duration_s=5.0, rng=rng)
            if _detected(tr.reading, baseline, noise):
                flagged += 1
        fp_rate[dev] = flagged / trials

    # True-positive sanity check: an active reader held at a close range.
    model = FieldModel(TEST_READER, rng=rng)
    tp_hits = 0
    for _ in range(trials):
        tr = tg.static_distance_trace(model, 4.0, duration_s=5.0)
        if _detected(tr.reading, baseline, noise):
            tp_hits += 1
    tp_rate = tp_hits / trials

    labels = [d.replace("_", "\n") for d in benign] + ["active\nreader\n(4 cm)"]
    values = [fp_rate[d] * 100 for d in benign] + [tp_rate * 100]
    colors = [plots.ORANGE] * len(benign) + [plots.GREEN]

    fig, ax = plots.plt.subplots(figsize=(6.8, 4.2))
    bars = ax.bar(labels, values, color=colors)
    for b, v in zip(bars, values):
        ax.text(b.get_x() + b.get_width() / 2, v + 1.5, f"{v:.0f}%",
                ha="center", va="bottom", fontsize=9)
    ax.set_ylabel("Flagged as reader present (%)")
    ax.set_title("False positives on benign NFC vs true positive on a reader")
    ax.set_ylim(0, 110)
    paths = plots.save(fig, FIG_DIR, "false_positive")

    return dict(false_positive_rate=fp_rate, true_positive_rate=tp_rate,
                overall_fp_rate=float(np.mean(list(fp_rate.values()))), figures=paths)


# --- Study 4: clean vs compromised (booth A/B) -------------------------------
def study_clean_vs_compromised(rng, baseline):
    model = FieldModel(TEST_READER, rng=rng)
    absent_model = FieldModel(FieldParams(power=1.0, noise_floor=TEST_READER.noise_floor,
                                          noise_sigma=TEST_READER.noise_sigma), rng=rng)

    clean = tg.absent_trace(absent_model, duration_s=6.0)
    comp = tg.approach_trace(model, start_cm=25.0, closest_cm=1.5, duration_s=6.0)

    det_c = Detector(); det_c.force_calibration(baseline, TEST_READER.noise_sigma)
    det_k = Detector(); det_k.force_calibration(baseline, TEST_READER.noise_sigma)
    prox_clean = [det_c.push(r).proximity_pct for r in clean.reading]
    states_comp = [det_k.push(r) for r in comp.reading]
    prox_comp = [s.proximity_pct for s in states_comp]

    fig, (ax1, ax2) = plots.plt.subplots(2, 1, figsize=(6.8, 5.4), sharex=True)
    ax1.plot(clean.t, clean.reading, color=plots.MUTED, lw=1.6, label="clean box (empty)")
    ax1.plot(comp.t, comp.reading, color=plots.BLUE, lw=1.8, label="compromised box (reader)")
    ax1.axhline(baseline, color=plots.INK, ls=":", lw=1.0, label="baseline")
    ax1.set_ylabel("Raw reading")
    ax1.set_title("Clean vs compromised sweep (A/B booth mode)")
    ax1.legend(loc="upper right")

    ax2.plot(clean.t, prox_clean, color=plots.MUTED, lw=1.6, label="clean")
    ax2.plot(comp.t, prox_comp, color=plots.BLUE, lw=1.8, label="compromised")
    ax2.set_ylabel("Proximity (%)")
    ax2.set_xlabel("Time (s)")
    ax2.set_ylim(-3, 103)
    ax2.legend(loc="upper right")
    paths = plots.save(fig, FIG_DIR, "clean_vs_compromised")

    return dict(clean_max_prox=float(np.max(prox_clean)),
                compromised_max_prox=float(np.max(prox_comp)), figures=paths)


# --- Extra: intermittent tracking --------------------------------------------
def study_intermittent(rng, baseline, noise):
    model = FieldModel(TEST_READER, rng=rng)
    tr = tg.intermittent_trace(model, distance_cm=6.0, duration_s=8.0, on_s=0.6, off_s=0.9)

    det = Detector(); det.force_calibration(baseline, noise)
    states = [det.push(r) for r in tr.reading]
    pred = np.array([s.present for s in states])
    truth = tr.label

    tp = int(np.sum(pred & truth)); fp = int(np.sum(pred & ~truth))
    fn = int(np.sum(~pred & truth)); tn = int(np.sum(~pred & ~truth))
    precision = tp / (tp + fp) if (tp + fp) else 0.0
    recall = tp / (tp + fn) if (tp + fn) else 0.0
    final_char = states[-1].characterization

    fig, ax = plots.plt.subplots(figsize=(6.8, 4.0))
    ax.plot(tr.t, tr.reading, color=plots.BLUE, lw=1.4, label="raw reading")
    ax.fill_between(tr.t, 0, tr.reading.max() * 1.05, where=truth, color=plots.GREEN,
                    alpha=0.12, label="reader ON (truth)")
    ax.fill_between(tr.t, 0, tr.reading.max() * 1.05, where=pred, color=plots.ORANGE,
                    alpha=0.18, step="mid", label="detector says present")
    ax.set_xlabel("Time (s)"); ax.set_ylabel("Raw reading")
    ax.set_title(f"Intermittent reader tracking — characterized as '{final_char}'")
    ax.legend(loc="upper right")
    paths = plots.save(fig, FIG_DIR, "intermittent_tracking")

    return dict(precision=round(precision, 3), recall=round(recall, 3),
                characterization=final_char, confusion=dict(tp=tp, fp=fp, fn=fn, tn=tn),
                figures=paths)


def _write_markdown(results):
    r2 = results["detection_range"]; fp = results["false_positives"]
    im = results["intermittent"]
    lines = [
        "# SkimGuard — Evaluation Results",
        "",
        "_Generated by `python -m eval.run_eval`. Do not edit by hand — rerun to refresh._",
        "",
        f"Random seed: **{results['seed']}**  ·  test reader power: **{TEST_READER.power}**",
        "",
        "## Headline numbers",
        "",
        "| Metric | Value |",
        "|---|---|",
        f"| Reliable detection range (P ≥ 90%) | **{r2['reliable_range_cm']} cm** |",
        f"| Marginal range (P ≥ 50%) | {r2['range_50pct_cm']} cm |",
        f"| True-positive rate (active reader @ 4 cm) | {fp['true_positive_rate']*100:.0f}% |",
        f"| Overall false-positive rate (benign NFC) | {fp['overall_fp_rate']*100:.1f}% |",
        f"| Intermittent tracking precision / recall | {im['precision']} / {im['recall']} |",
        "",
        "## False positives by benign device",
        "",
        "| Device | Flagged as reader |",
        "|---|---|",
    ]
    for dev, rate in fp["false_positive_rate"].items():
        lines.append(f"| {dev.replace('_', ' ')} | {rate*100:.0f}% |")
    lines += [
        "",
        "## Figures",
        "",
        "![Field vs distance](../figures/field_vs_distance.png)",
        "![Detection range](../figures/detection_range.png)",
        "![False positives](../figures/false_positive.png)",
        "![Clean vs compromised](../figures/clean_vs_compromised.png)",
        "![Intermittent tracking](../figures/intermittent_tracking.png)",
        "",
        "See `docs/prior_art.md` for how these limits line up with the honest",
        "scope of the technique, and `docs/threat_model.md` for what a quiet",
        "reading does and does not guarantee.",
        "",
    ]
    os.makedirs(os.path.dirname(RESULTS_MD), exist_ok=True)
    with open(RESULTS_MD, "w") as f:
        f.write("\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description="Run the SkimGuard measurement study.")
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    plots.apply_style()
    rng = np.random.default_rng(args.seed)
    baseline, noise = _clean_baseline(rng)

    print(f"[eval] seed={args.seed}  baseline={baseline:.1f}  noise={noise:.2f}")
    results = {"seed": args.seed, "baseline": baseline, "noise": noise}

    print("[eval] 1/5 field vs distance ...")
    results["field_vs_distance"] = study_field_vs_distance(rng, baseline)
    print("[eval] 2/5 detection range ...")
    results["detection_range"] = study_detection_range(rng, baseline, noise)
    print("[eval] 3/5 false positives ...")
    results["false_positives"] = study_false_positives(rng, baseline, noise)
    print("[eval] 4/5 clean vs compromised ...")
    results["clean_vs_compromised"] = study_clean_vs_compromised(rng, baseline)
    print("[eval] 5/5 intermittent tracking ...")
    results["intermittent"] = study_intermittent(rng, baseline, noise)

    os.makedirs(os.path.dirname(RESULTS_JSON), exist_ok=True)
    with open(RESULTS_JSON, "w") as f:
        json.dump(results, f, indent=2)
    _write_markdown(results)

    r = results["detection_range"]; fp = results["false_positives"]
    print("\n=== SkimGuard eval summary ===")
    print(f"  reliable range : {r['reliable_range_cm']} cm (P>=90%)")
    print(f"  50% range      : {r['range_50pct_cm']} cm")
    print(f"  TP @ 4cm       : {fp['true_positive_rate']*100:.0f}%")
    print(f"  overall FP     : {fp['overall_fp_rate']*100:.1f}%")
    print(f"  results        : {RESULTS_JSON}, {RESULTS_MD}")
    print(f"  figures        : {FIG_DIR}/*.png,*.svg")


if __name__ == "__main__":
    main()
