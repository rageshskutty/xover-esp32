import contextlib
import glob
import io
import json
import math
import os
import unittest

import numpy as np

import protection_oracle as oracle
from protection_oracle import (
    ZERO_FLOOR_DB, conservative_bound_db, evaluate_config, filter_response,
    response_magnitude,
)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CORNER_Q_DB = 20.0 * math.log10(0.7071)  # |H(fc)| of a 2nd-order filter equals Q
F1K = [1000.0]


def filt(kind, f_hz=1000.0, q=0.7071, gain_db=0.0):
    return {"type": kind, "f_hz": f_hz, "q": q, "gain_db": gain_db}


def output(oid, mix, filters=(), gain_db=0.0, slot=0):
    return {"id": oid, "slot": slot,
            "in": [{"channel": c, "weight": w} for c, w in mix],
            "filters": list(filters), "gain_db": gain_db,
            "polarity": 1, "delay_samples": 0}


def config(outputs, pre=(), protection=None):
    cfg = {"v": 1, "fs": 48000, "inputs": ["L", "R"],
           "pre": list(pre), "outputs": list(outputs)}
    if protection is not None:
        cfg["protection"] = {"outputs": protection}
    return cfg


def record(oid, weighting=()):
    return {"id": oid, "weighting": {"filters": list(weighting)},
            "peak_threshold_dbfs": -6.0, "hold_ms": 20.0,
            "thermal": [{"rms_threshold_dbfs": -12.0, "rms_tau_ms": 250.0}],
            "release_ms": 150.0, "ceiling_dbfs": -1.0}


def db(x):
    return 20.0 * math.log10(x)


class VectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.path.join(ROOT, "tools", "protection_oracle_vectors.json")
        with open(path, encoding="utf-8") as fh:
            cls.vectors = json.load(fh)

    def test_filter_vectors(self):
        for v in self.vectors["filters"]:
            with self.subTest(v["name"]):
                got = 20.0 * np.log10(np.abs(
                    filter_response(v["filters"], v["frequencies_hz"], 48000.0)))
                if "max_db" in v:
                    self.assertTrue(np.all(got <= v["max_db"]), got)
                else:
                    np.testing.assert_allclose(got, v["expected_db"],
                                               rtol=0.0, atol=v["tol_db"])

    def test_bound_vectors(self):
        for v in self.vectors["bounds"]:
            with self.subTest(v["name"]):
                got = conservative_bound_db(v["weights"], v["magnitudes"])
                np.testing.assert_allclose(got, v["expected_bound_db"],
                                           rtol=0.0, atol=1e-9)

    def test_main_runs_all_vectors(self):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            oracle.main()
        self.assertIn("PASS", buf.getvalue())


class ConfigEvaluationTests(unittest.TestCase):
    def test_output_gain_is_applied(self):
        got = evaluate_config(config([output("o", [("L", 1.0)], gain_db=-6.0)], ), F1K)
        np.testing.assert_allclose(got["o"]["bound_db"], [-6.0], atol=1e-9)

    def test_pre_and_output_filters_are_both_included(self):
        pre = [{"channel": "L", "filters": [filt("hp2")]}]
        out = output("o", [("L", 1.0), ("R", 1.0)])
        got = evaluate_config(config([out], pre=pre), F1K)
        np.testing.assert_allclose(got["o"]["bound_db"],
                                   [db(0.7071 + 1.0)], atol=0.001)

    def test_output_filter_applies_to_every_mixed_input(self):
        out = output("o", [("L", 1.0), ("R", 1.0)], [filt("lp2")])
        got = evaluate_config(config([out]), F1K)
        np.testing.assert_allclose(got["o"]["bound_db"],
                                   [db(2.0 * 0.7071)], atol=0.001)

    def test_difference_output_gets_no_cancellation_credit(self):
        out = output("o", [("L", 1.0), ("R", -1.0)])
        got = evaluate_config(config([out]), [100.0, 1000.0])
        np.testing.assert_allclose(got["o"]["bound_db"], [db(2.0)] * 2, atol=1e-9)

    def test_weighting_is_reported_apart_from_the_signal_path(self):
        cfg = config([output("o", [("L", 1.0)])],
                     protection=[record("o", [filt("hp2")])])
        got = evaluate_config(cfg, F1K)
        np.testing.assert_allclose(got["o"]["bound_db"], [0.0], atol=1e-9)
        np.testing.assert_allclose(got["o"]["weighting_db"], [CORNER_Q_DB], atol=0.001)

    def test_empty_weighting_is_flat(self):
        cfg = config([output("o", [("L", 1.0)])], protection=[record("o")])
        got = evaluate_config(cfg, [100.0, 1000.0])
        np.testing.assert_allclose(got["o"]["weighting_db"], [0.0, 0.0], atol=1e-12)

    def test_unprovisioned_config_has_no_weighting_report(self):
        got = evaluate_config(config([output("o", [("L", 1.0)])]), F1K)
        self.assertIsNone(got["o"]["weighting_db"])

    def test_example_configs_stay_within_their_mix_and_gain_ceiling(self):
        grid = np.geomspace(20.0, 24000.0, 200)
        paths = [p for p in sorted(glob.glob(os.path.join(ROOT, "configs", "*.json")))
                 if os.path.basename(p) != "schema.json"]
        self.assertTrue(paths)
        for path in paths:
            with open(path, encoding="utf-8") as fh:
                cfg = json.load(fh)
            got = evaluate_config(cfg, grid)
            for out in cfg["outputs"]:
                with self.subTest(os.path.basename(path), output=out["id"]):
                    bound = got[out["id"]]["bound_db"]
                    ceiling = db(sum(abs(m["weight"]) for m in out["in"])) + out["gain_db"]
                    self.assertTrue(np.all(np.isfinite(bound)))
                    self.assertTrue(np.all(bound <= ceiling + 1e-4), bound.max())


class RejectionTests(unittest.TestCase):
    def check(self, filters, pattern):
        with self.assertRaisesRegex(ValueError, pattern):
            filter_response(filters, F1K, 48000.0)

    def test_unknown_filter_type(self):
        self.check([filt("bessel")], "unsupported filter type")

    def test_raw_biquad_block_is_not_a_filter(self):
        self.check([{"t": "biquad", "k": "hp", "f_hz": 100.0, "q": 0.7}],
                   "unsupported filter type")

    def test_frequency_bounds_follow_the_schema(self):
        self.check([filt("hp2", f_hz=19.9)], "f_hz must")
        self.check([filt("hp2", f_hz=21600.0)], "f_hz must")
        self.check([filt("hp2", f_hz=float("nan"))], "f_hz must")

    def test_q_bounds_follow_the_schema(self):
        self.check([filt("peq", q=0.05)], r"q must")
        self.check([filt("peq", q=20.5)], r"q must")

    def test_gain_bounds_follow_the_schema(self):
        self.check([filt("peq", gain_db=24.5, q=1.0)], "gain_db must")
        self.check([filt("lshelf", gain_db=-24.5)], "gain_db must")

    def test_boost_is_capped_at_plus_12_db(self):
        # Q2: same cap as dsp_design_peq / dsp_design_shelf; cuts stay at -24 dB.
        for kind in ("peq", "lshelf", "hshelf"):
            with self.subTest(kind):
                self.check([filt(kind, gain_db=12.1, q=1.0)], "gain_db must")
                self.assertIsNotNone(oracle.design_filter(filt(kind, gain_db=12.0, q=1.0), 48000.0))
                self.assertIsNotNone(oracle.design_filter(filt(kind, gain_db=-24.0, q=1.0), 48000.0))

    def test_gain_on_a_type_without_gain_is_rejected(self):
        for kind in ("lp1", "hp1", "lp2", "hp2", "ap1", "ap2", "notch"):
            with self.subTest(kind):
                self.check([filt(kind, gain_db=3.0)], "gain_db is not defined")

    def test_response_grid_must_stay_inside_nyquist(self):
        for grid in ([0.0], [24001.0], []):
            with self.subTest(grid=grid):
                with self.assertRaisesRegex(ValueError, "frequencies"):
                    filter_response([], grid, 48000.0)

    def test_config_version_and_sample_rate(self):
        good = config([output("o", [("L", 1.0)])])
        for key, value in (("v", 2), ("fs", 44100)):
            with self.subTest(key):
                with self.assertRaisesRegex(ValueError, "unsupported"):
                    evaluate_config(dict(good, **{key: value}), F1K)

    def test_duplicate_output_ids_and_pre_channels_are_rejected(self):
        twin = [output("o", [("L", 1.0)]), output("o", [("R", 1.0)], slot=1)]
        with self.assertRaisesRegex(ValueError, "duplicate output id"):
            evaluate_config(config(twin), F1K)
        pre = [{"channel": "L", "filters": []}, {"channel": "L", "filters": []}]
        with self.assertRaisesRegex(ValueError, "duplicate pre channel"):
            evaluate_config(config([output("o", [("L", 1.0)])], pre=pre), F1K)

    def test_output_gain_must_be_in_range(self):
        with self.assertRaisesRegex(ValueError, "gain_db must"):
            evaluate_config(config([output("o", [("L", 1.0)], gain_db=12.5)]), F1K)

    def test_mix_weight_must_be_finite_and_non_zero(self):
        for weight in (0.0, float("inf"), float("nan")):
            with self.subTest(weight=weight):
                with self.assertRaisesRegex(ValueError, "weight"):
                    evaluate_config(config([output("o", [("L", weight)])]), F1K)

    def test_negative_magnitude_and_shape_errors(self):
        with self.assertRaisesRegex(ValueError, "non-negative"):
            conservative_bound_db([1.0], [[-0.1]])
        with self.assertRaisesRegex(ValueError, "dimensions"):
            conservative_bound_db([1.0, 1.0], [[1.0]])
        with self.assertRaisesRegex(ValueError, "finite"):
            conservative_bound_db([1.0], [[float("nan")]])

    def test_zero_floor_is_finite(self):
        got = conservative_bound_db([1.0], [[0.0]])
        self.assertEqual(float(got[0]), ZERO_FLOOR_DB)


class RetiredApiTests(unittest.TestCase):
    def test_curve_knot_comparator_is_gone(self):
        for name in ("interpolate_limit_curve", "compare_output", "compare_config",
                     "chain_response", "_design_biquad"):
            with self.subTest(name):
                self.assertFalse(hasattr(oracle, name))

    def test_sos_response_helper_still_matches_unity(self):
        got = response_magnitude([[1.0, 0.0, 0.0, 1.0, 0.0, 0.0]],
                                 [10.0, 1000.0, 20000.0], 48000.0)
        np.testing.assert_allclose(got, [1.0, 1.0, 1.0], atol=1e-12)


if __name__ == "__main__":
    unittest.main()
