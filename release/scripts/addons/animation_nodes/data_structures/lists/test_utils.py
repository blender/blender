from unittest import TestCase
from . utils import predictSliceLength, makeStepPositive

class TestPredictSliceLength(TestCase):
    def testSimple(self):
        self.check(0, 10, 1)
        self.check(0, 10, 2)
        self.check(0, 10, 11)

        self.check(1, 10, 1)
        self.check(2, 10, 1)
        self.check(11, 10, 1)

        self.check(10, 0, -1)
        self.check(10, 0, -2)
        self.check(10, 3, -3)
        self.check(3, 3, -3)

    def check(self, start, stop, step):
        real = len(tuple(range(start, stop, step)))
        calculated = predictSliceLength(start, stop, step)
        self.assertEqual(real, calculated)

class TestMakeStepPositive(TestCase):
    def test(self):
        self.assertEqual(makeStepPositive(9, -1, -2), (1, 10, 2))
        self.assertEqual(makeStepPositive(10, 5, -1), (6, 11, 1))
