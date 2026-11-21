import unittest

import numpy as np

from protection_oracle import (
    ZERO_FLOOR_DB, compare_config, compare_output, interpolate_limit_curve,
    response_magnitude,
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

    def test_scipy_sos_response_matches_unity(self):
        got = response_magnitude([[1.0, 0.0, 0.0, 1.0, 0.0, 0.0]],
                                 [10.0, 1000.0, 20000.0], 48000.0)
        np.testing.assert_allclose(got, [1.0, 1.0, 1.0], atol=1e-12)


if __name__ == "__main__":
    unittest.main()
