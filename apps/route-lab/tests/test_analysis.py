import unittest
from itertools import repeat

from analysis.analyse import paired_difference


def outcomes(counts):
    for outcome, count in zip(((True, True), (True, False), (False, True), (False, False)), counts):
        yield from repeat(outcome, count)


class PairedDifferenceTests(unittest.TestCase):
    def test_published_intervals(self):
        # Newcombe (1998), Table III, method 10.
        for counts, expected in (
            ((20, 12, 2, 16), (0.0562, 0.3292)),
            ((36, 14, 0, 0), (0.1528, 0.4167)),
            ((0, 30, 0, 0), (0.8395, 1.0000)),
            ((27, 0, 0, 27), (-0.0351, 0.0351)),
        ):
            with self.subTest(counts=counts):
                _, low, high = paired_difference(outcomes(counts))
                self.assertAlmostEqual(low, expected[0], places=4)
                self.assertAlmostEqual(high, expected[1], places=4)

    def test_swapping_policies_reverses_the_interval(self):
        estimate, low, high = paired_difference(outcomes((20, 12, 2, 16)))
        reverse_estimate, reverse_low, reverse_high = paired_difference(outcomes((20, 2, 12, 16)))
        self.assertAlmostEqual(estimate, 0.2)
        self.assertAlmostEqual(reverse_estimate, -estimate)
        self.assertAlmostEqual(reverse_low, -high)
        self.assertAlmostEqual(reverse_high, -low)
