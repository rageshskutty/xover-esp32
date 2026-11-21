import unittest

import numpy as np

from protection_oracle import (
    ZERO_FLOOR_DB, compare_config, compare_output, interpolate_limit_curve,
    response_magnitude, evaluate_config, chain_response,
)


class ProtectionOracleTests(unittest.TestCase):
    def test_interpolates_linear_db_on_log_frequency(self):
        got = interpolate_limit_curve([10.0, 100.0, 1000.0],
                                       [10.0, 1000.0], [-20.0, 0.0])
        np.testing.assert_allclose(got, [-20.0, -10.0, 0.0], atol=1e-12)

    def test_curve_does_not_extrapolate(self):
        with self.assertRaisesRegex(ValueError, "does not cover"):
            interpolate_limit_curve([9.0], [10.0, 100.0], [-20.0, 0.0])

    def test_duplicate_curve_knots_rejected(self):
        with self.assertRaisesRegex(ValueError, "strictly increasing"):
            interpolate_limit_curve([10.0], [10.0, 10.0], [-20.0, 0.0])

    def test_nonfinite_values_rejected(self):
        with self.assertRaisesRegex(ValueError, "finite"):
            compare_output([1.0], [[float("nan")]], [0.0], [100.0], "out")

    def test_negative_response_rejected(self):
        with self.assertRaisesRegex(ValueError, "non-negative"):
            compare_output([1.0], [[-0.1]], [0.0], [100.0], "out")

    def test_dimensions_must_match(self):
        with self.assertRaisesRegex(ValueError, "dimensions disagree"):
            compare_output([1.0], [[1.0, 1.0]], [0.0], [100.0], "out")

    def test_zero_floor_is_finite(self):
        got = compare_output([1.0], [[0.0]], [ZERO_FLOOR_DB], [100.0], "out")
        self.assertTrue(got["pass"])

    def test_first_violation_output_order(self):
        outputs = [
            {"id": "first", "weights": [1.0], "magnitudes": [[2.0]],
             "limit_db": [0.0]},
            {"id": "second", "weights": [1.0], "magnitudes": [[3.0]],
             "limit_db": [0.0]},
        ]
        self.assertEqual(compare_config(outputs, [100.0])["output_id"], "first")

    def test_static_sum_fixture_is_calculated_from_chain_response(self):
        config = {
            "v": 1, "fs": 48000, "inputs": ["L", "R"],
            "outputs": [{
                "id": "out_a", "slot": 0,
                "in": [{"channel": "L", "weight": 1.0}],
                "chain": [{"t": "gain", "db": 1.0}],
            }],
        }
        curve = {"out_a": {"frequencies_hz": [10.0, 20000.0],
                           "limit_db": [0.0, 0.0]}}
        got = evaluate_config(config, curve, [10.0, 100.0, 1000.0, 20000.0])
        self.assertFalse(got["pass"])
        self.assertEqual(got["output_id"], "out_a")
        self.assertAlmostEqual(got["bound_db"], 1.0, places=6)

    def test_exact_curve_knots_are_added_to_evaluation_grid(self):
        config = {
            "v": 1, "fs": 48000, "inputs": ["L"],
            "outputs": [{
                "id": "out", "slot": 0,
                "in": [{"channel": "L", "weight": 1.0}],
                "chain": [{"t": "gain", "db": 1.0}],
            }],
        }
        curve = {"out": {"frequencies_hz": [10.0, 100.0, 20000.0],
                         "limit_db": [2.0, 0.0, 2.0]}}
        got = evaluate_config(config, curve, [10.0, 20000.0])
        self.assertFalse(got["pass"])
        self.assertEqual(got["frequency_hz"], 100.0)

    def test_pre_chain_and_output_chain_are_both_included(self):
        config = {
            "v": 1, "fs": 48000, "inputs": ["L"],
            "pre": [{"channel": "L", "chain": [{"t": "gain", "db": -3.0}]}],
            "outputs": [{
                "id": "out", "slot": 0,
                "in": [{"channel": "L", "weight": 1.0}],
                "chain": [{"t": "gain", "db": -3.0}],
            }],
        }
        curve = {"out": {"frequencies_hz": [10.0, 20000.0],
                         "limit_db": [-5.0, -5.0]}}
        got = evaluate_config(config, curve, [10.0, 1000.0, 20000.0])
        self.assertTrue(got["pass"])

    def test_highpass_response_is_frequency_dependent(self):
        chain = [{"t": "biquad", "k": "hp", "f_hz": 20.0, "q": 0.707}]
        h = chain_response(chain, [10.0, 20.0, 1000.0], 48000.0)
        mag = np.abs(h)
        self.assertLess(mag[0], mag[1])
        self.assertLess(mag[1], mag[2])
        self.assertTrue(np.all(np.isfinite(mag)))

    def test_supported_static_blocks_have_finite_response(self):
        chain = [
            {"t": "biquad", "k": "peq", "f_hz": 1000.0, "q": 0.7, "db": -6.0},
            {"t": "biquad", "k": "notch", "f_hz": 2000.0, "q": 2.0},
            {"t": "biquad", "k": "allpass", "f_hz": 500.0, "q": 0.8},
            {"t": "delay", "us": 125.0},
            {"t": "polarity", "invert": True},
            {"t": "gain", "db": -3.0},
        ]
        h = chain_response(chain, [20.0, 1000.0, 10000.0], 48000.0)
        self.assertTrue(np.all(np.isfinite(h.real)))
        self.assertTrue(np.all(np.isfinite(h.imag)))

    def test_protected_min_frequency_requires_subsonic_hp(self):
        config = {
            "v": 1, "fs": 48000, "inputs": ["L"],
            "outputs": [{
                "id": "out", "slot": 0,
                "in": [{"channel": "L", "weight": 1.0}],
                "chain": [{"t": "gain", "db": 0.0}],
            }],
        }
        curve = {"out": {"frequencies_hz": [10.0, 20000.0],
                         "limit_db": [-200.0, 0.0], "min_freq_hz": 20.0}}
        with self.assertRaisesRegex(ValueError, "lacks required subsonic"):
            evaluate_config(config, curve, [10.0, 100.0, 20000.0])

    def test_sos_sample_rate_mismatch_rejected(self):
        with self.assertRaisesRegex(ValueError, "sample-rate mismatch"):
            chain_response([{"t": "sos", "fs": 44100,
                             "b": [1.0, 0.0, 0.0], "a": [0.0, 0.0]}],
                           [100.0], 48000.0)

    def test_duplicate_output_ids_rejected(self):
        config = {
            "v": 1, "fs": 48000, "inputs": ["L"],
            "outputs": [
                {"id": "same", "slot": 0, "in": [{"channel": "L", "weight": 1.0}], "chain": [{"t": "gain", "db": 0.0}]},
                {"id": "same", "slot": 1, "in": [{"channel": "L", "weight": 1.0}], "chain": [{"t": "gain", "db": 0.0}]},
            ],
        }
        curve = {"same": {"frequencies_hz": [10.0, 20000.0], "limit_db": [0.0, 0.0]}}
        with self.assertRaisesRegex(ValueError, "IDs must match"):
            evaluate_config(config, curve, [10.0, 20000.0])

    def test_scipy_sos_response_matches_unity(self):
        got = response_magnitude([[1.0, 0.0, 0.0, 1.0, 0.0, 0.0]],
                                 [10.0, 1000.0, 20000.0], 48000.0)
        np.testing.assert_allclose(got, [1.0, 1.0, 1.0], atol=1e-12)


if __name__ == "__main__":
    unittest.main()
