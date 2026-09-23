"""Host tests for the SkimGuard simulator + reference detector.

Run: pytest -q   (from the repo root, with numpy installed)

Author / sole contributor: Krishita Sanjay Choksi.
"""
import numpy as np
import pytest

from sim.field_model import FieldModel, FieldParams, FULL_SCALE
from sim import trace_generator as tg
from eval.detector import Detector, PARAMS, run_detector
from eval.traceio import write_trace, read_trace


def test_field_monotonic_in_distance():
    """Field strength must fall monotonically as distance grows."""
    m = FieldModel(FieldParams(noise_sigma=0.0))
    d = np.linspace(0.5, 40, 200)
    h = m.field_strength(d)
    assert np.all(np.diff(h) < 0)


def test_reading_bounds():
    m = FieldModel(FieldParams())
    r = m.reading(np.linspace(0, 30, 100), active=True)
    assert r.min() >= 0 and r.max() <= FULL_SCALE


def test_absent_reads_floor():
    m = FieldModel(FieldParams(noise_sigma=0.0))
    r = np.atleast_1d(m.reading(np.zeros(50), active=False))
    assert np.allclose(r, FieldParams().noise_floor, atol=1.0)


def test_detector_matches_config_constants():
    """Reference detector uses the same constants the firmware header defines."""
    assert PARAMS["ON_SIGMA"] == 6.0
    assert PARAMS["CALIB_SAMPLES"] == 20


def test_detector_calibrates_then_clear_on_absent():
    m = FieldModel(FieldParams(), rng=np.random.default_rng(1))
    tr = tg.absent_trace(m, duration_s=10.0)
    states = run_detector(tr.reading)
    # After calibration, an absent trace should not report present.
    warm = states[PARAMS["CALIB_SAMPLES"] + 5 :]
    assert not any(s.present for s in warm)


def test_detector_detects_close_reader():
    m = FieldModel(FieldParams(), rng=np.random.default_rng(2))
    det = Detector()
    det.force_calibration(18.0, 6.0)
    tr = tg.static_distance_trace(m, 3.0, duration_s=3.0)
    presents = [det.push(r).present for r in tr.reading]
    assert any(presents)
    assert max(det.push(r).proximity_pct for r in tr.reading) > 30


def test_proximity_increases_on_approach():
    m = FieldModel(FieldParams(noise_sigma=1.0), rng=np.random.default_rng(3))
    det = Detector()
    det.force_calibration(18.0, 6.0)
    tr = tg.approach_trace(m, start_cm=25, closest_cm=1.0, duration_s=6.0)
    prox = np.array([det.push(r).proximity_pct for r in tr.reading])
    # Peak proximity should occur near the middle (closest approach), not the ends.
    peak = int(np.argmax(prox))
    assert 0.25 * len(prox) < peak < 0.75 * len(prox)


def test_benign_devices_rarely_flag():
    rng = np.random.default_rng(4)
    flagged = 0
    trials = 40
    for _ in range(trials):
        tr = tg.benign_device_trace("transit_card", rng=rng)
        det = Detector()
        det.force_calibration(18.0, 6.0)
        if any(det.push(r).present for r in tr.reading):
            flagged += 1
    assert flagged / trials < 0.1  # honest, low false-positive rate


def test_trace_roundtrip(tmp_path):
    m = FieldModel(FieldParams(), rng=np.random.default_rng(5))
    tr = tg.approach_trace(m)
    p = tmp_path / "t.csv"
    write_trace(str(p), tr)
    back = read_trace(str(p))
    assert np.allclose(back.reading, tr.reading)
    assert back.label.tolist() == tr.label.tolist()


def test_intermittent_characterization():
    m = FieldModel(FieldParams(), rng=np.random.default_rng(6))
    det = Detector()
    det.force_calibration(18.0, 6.0)
    tr = tg.intermittent_trace(m, distance_cm=6.0, duration_s=12.0, on_s=0.6, off_s=0.9)
    chars = [det.push(r).characterization for r in tr.reading]
    assert "intermittent" in chars


def test_confidence_tracks_signal():
    det = Detector()
    det.force_calibration(18.0, 6.0)
    m = FieldModel(FieldParams(noise_sigma=1.0), rng=np.random.default_rng(11))
    absent = det.push(18.0).confidence_pct
    near = None
    for r in tg.static_distance_trace(m, 3.0, duration_s=2.0).reading:
        near = det.push(r).confidence_pct
    assert absent < 5.0
    assert near > 50.0


def test_distance_estimate_monotonic_and_valid():
    near, vn = Detector.estimate_distance_cm(600.0)
    far, vf = Detector.estimate_distance_cm(80.0)
    assert vn and vf
    assert near < far
    _, v_tiny = Detector.estimate_distance_cm(1.0)
    assert not v_tiny


def test_distance_estimate_roughly_matches_truth():
    """For the nominal reference reader, the estimate should track true distance."""
    m = FieldModel(FieldParams(noise_sigma=0.0), rng=np.random.default_rng(12))
    for true_d in (4.0, 7.0, 10.0):
        det = Detector()
        det.force_calibration(18.0, 6.0)
        est = None
        for r in tg.static_distance_trace(m, true_d, duration_s=2.0).reading:
            st = det.push(r)
            if st.distance_valid:
                est = st.distance_cm_est
        assert est is not None
        assert abs(est - true_d) < 3.5  # within a few cm of ground truth


def test_sensitivity_changes_threshold():
    lo = Detector(); lo.force_calibration(18.0, 6.0); lo.set_sensitivity(PARAMS["SENS_LOW"])
    hi = Detector(); hi.force_calibration(18.0, 6.0); hi.set_sensitivity(PARAMS["SENS_HIGH"])
    assert lo.on_threshold > hi.on_threshold


def test_drift_compensation_suppresses_slow_rise():
    det = Detector()
    det.force_calibration(18.0, 6.0)
    ever = False
    for i in range(400):
        v = 18.0 + 42.0 * (i / 400.0)
        if det.push(v).present:
            ever = True
    assert not ever


def test_poll_rate_estimate():
    m = FieldModel(FieldParams(), rng=np.random.default_rng(13))
    det = Detector()
    det.force_calibration(18.0, 6.0)
    tr = tg.intermittent_trace(m, distance_cm=6.0, duration_s=14.0, on_s=0.6, off_s=0.9)
    poll = 0.0
    for r in tr.reading:
        st = det.push(r)
        if st.poll_hz > 0:
            poll = st.poll_hz
    # period 1.5 s -> ~0.667 Hz
    assert 0.4 < poll < 0.95


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__, "-q"]))
