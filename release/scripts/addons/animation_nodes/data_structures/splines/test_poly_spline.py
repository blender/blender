from mathutils import Vector
from unittest import TestCase
from . poly_spline import PolySpline

class TestInitialisation(TestCase):
    def testNormal(self):
        spline = PolySpline()
        self.assertFalse(spline.cyclic)
        self.assertEqual(spline.type, "POLY")
        self.assertEqual(len(spline.points), 0)

class TestAppendPoint(TestCase):
    def setUp(self):
        self.spline = PolySpline()

    def testNormal(self):
        self.spline.appendPoint((0, 0, 0))
        self.spline.appendPoint((1, 1, 1))
        points = self.spline.points
        self.assertEqual(len(points), 2)
        self.assertEqual(points[0], Vector((0, 0, 0)))
        self.assertEqual(points[1], Vector((1, 1, 1)))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.spline.appendPoint("abc")

    def testTooLongVector(self):
        with self.assertRaises(TypeError):
            self.spline.appendPoint((0, 1, 2, 3))

class TestGetLength(TestCase):
    def testEmptySpline(self):
        spline = PolySpline()
        self.assertEqual(spline.getLength(), 0)

    def testSplineWithOnePoint(self):
        spline = PolySpline()
        spline.appendPoint((1, 2, 3))
        self.assertEqual(spline.getLength(), 0)

    def testSimple(self):
        spline = PolySpline()
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((3, 4, 0))
        self.assertAlmostEqual(spline.getLength(), 5)

    def testMultiplePoints(self):
        spline = PolySpline()
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((2, 4, 6))
        spline.appendPoint((2, 10, 8))
        self.assertAlmostEqual(spline.getLength(), 13.80787009, places = 5)

    def testSimpleCyclic(self):
        spline = PolySpline()
        spline.cyclic = True
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((3, 4, 0))
        self.assertAlmostEqual(spline.getLength(), 10)

    def testMultiplePointsCyclic(self):
        spline = PolySpline()
        spline.cyclic = True
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((2, 4, 6))
        spline.appendPoint((2, 10, 8))
        self.assertAlmostEqual(spline.getLength(), 26.76935149)

class TestEvaluate(TestCase):
    def setUp(self):
        self.spline = PolySpline()
        self.spline.appendPoint((0, 0, 0))
        self.spline.appendPoint((2, 4, 6))
        self.spline.appendPoint((2, 10, 8))

    def testZero(self):
        testEqual(self, self.spline.evaluate(0), (0, 0, 0))

    def testOne(self):
        testEqual(self, self.spline.evaluate(1), (2, 10, 8))

    def testMiddle(self):
        testEqual(self, self.spline.evaluate(0.25), (1, 2, 3))
        testEqual(self, self.spline.evaluate(0.50), (2, 4, 6))
        testEqual(self, self.spline.evaluate(0.75), (2, 7, 7))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.spline.evaluate("abc")

    def testInvalidValue(self):
        with self.assertRaises(ValueError):
            self.spline.evaluate(-1)
        with self.assertRaises(ValueError):
            self.spline.evaluate(1.5)

    def testEmptySpline(self):
        spline = PolySpline()
        with self.assertRaises(Exception):
            spline.evaluate(0)

    def testCyclic(self):
        self.spline.cyclic = True
        testEqual(self, self.spline.evaluate(0.0), (0, 0, 0))
        testEqual(self, self.spline.evaluate(1/3), (2, 4, 6))
        testEqual(self, self.spline.evaluate(0.5), (2, 7, 7))
        testEqual(self, self.spline.evaluate(2/3), (2, 10, 8))
        testEqual(self, self.spline.evaluate(5/6), (1, 5, 4))
        testEqual(self, self.spline.evaluate(1.0), (0, 0, 0))

class TestEvaluateTangent(TestCase):
    def setUp(self):
        self.spline = PolySpline()
        self.spline.appendPoint((0, 0, 0))
        self.spline.appendPoint((2, 4, 6))
        self.spline.appendPoint((2, 10, 8))

    def testZero(self):
        testEqual(self, self.spline.evaluateTangent(0), (2, 4, 6))

    def testOne(self):
        testEqual(self, self.spline.evaluateTangent(1), (0, 6, 2))

    def testMiddle(self):
        testEqual(self, self.spline.evaluateTangent(0.25), (2, 4, 6))
        testEqual(self, self.spline.evaluateTangent(0.50), (0, 6, 2))
        testEqual(self, self.spline.evaluateTangent(0.75), (0, 6, 2))

    def testWrongType(self):
        with self.assertRaises(TypeError):
            self.spline.evaluateTangent("abc")

    def testInvalidValue(self):
        with self.assertRaises(ValueError):
            self.spline.evaluateTangent(-1)
        with self.assertRaises(ValueError):
            self.spline.evaluateTangent(1.5)

    def testEmptySpline(self):
        spline = PolySpline()
        with self.assertRaises(Exception):
            spline.evaluateTangent(0)

    def testCyclic(self):
        self.spline.cyclic = True
        testEqual(self, self.spline.evaluateTangent(0.0), (2, 4, 6))
        testEqual(self, self.spline.evaluateTangent(0.1), (2, 4, 6))
        testEqual(self, self.spline.evaluateTangent(1/3), (0, 6, 2))
        testEqual(self, self.spline.evaluateTangent(0.5), (0, 6, 2))
        testEqual(self, self.spline.evaluateTangent(2/3), (-2, -10, -8))
        testEqual(self, self.spline.evaluateTangent(0.8), (-2, -10, -8))
        testEqual(self, self.spline.evaluateTangent(1.0), (-2, -10, -8))

class TestGetSamples(TestCase):
    def setUp(self):
        self.spline = PolySpline()
        self.spline.appendPoint((0, 0, 0))
        self.spline.appendPoint((4, 0, 0))
        self.spline.appendPoint((4, 2, 0))
        self.spline.appendPoint((10, 2, 10))

    def testNormal(self):
        samples = self.spline.getSamples(4)
        self.assertEqual(len(samples), 4)
        testEqual(self, samples[0], (0, 0, 0))
        testEqual(self, samples[1], (4, 0, 0))
        testEqual(self, samples[2], (4, 2, 0))
        testEqual(self, samples[3], (10, 2, 10))

    def testZeroSamples(self):
        samples = self.spline.getSamples(0)
        self.assertEqual(len(samples), 0)

    def testOneSample(self):
        samples = self.spline.getSamples(1)
        self.assertEqual(len(samples), 1)
        testEqual(self, samples[0], (4, 1, 0))

    def testCyclicSimple(self):
        self.spline.cyclic = True
        samples = self.spline.getSamples(2)
        testEqual(self, samples[0], (0, 0, 0))
        testEqual(self, samples[1], (4, 2, 0))

    def testStartEnd(self):
        samples = self.spline.getSamples(5, start = 1/3, end = 2/3)
        testEqual(self, samples[0], (4, 0.0, 0))
        testEqual(self, samples[1], (4, 0.5, 0))
        testEqual(self, samples[2], (4, 1.0, 0))
        testEqual(self, samples[3], (4, 1.5, 0))
        testEqual(self, samples[4], (4, 2.0, 0))

    def testSwitchedParameters(self):
        samples = self.spline.getSamples(4, start = 1, end = 0.5)
        testEqual(self, samples[0], (10, 2, 10))
        testEqual(self, samples[1], (7, 2, 5))
        testEqual(self, samples[2], (4, 2, 0))
        testEqual(self, samples[3], (4, 1, 0))

    def testNegativeAmount(self):
        with self.assertRaises(ValueError):
            self.spline.getSamples(-1)

    def testParametersNotInRange(self):
        with self.assertRaises(ValueError):
            self.spline.getSamples(5, start = -0.5)
        with self.assertRaises(ValueError):
            self.spline.getSamples(5, start = 1.5)
        with self.assertRaises(ValueError):
            self.spline.getSamples(5, end = -0.5)
        with self.assertRaises(ValueError):
            self.spline.getSamples(5, end = 1.5)

    def testNotEvaluableSpline(self):
        spline = PolySpline()
        with self.assertRaises(Exception):
            spline.getSamples(5)
        spline.appendPoint((0, 0, 0))
        with self.assertRaises(Exception):
            spline.getSamples(5)

class TestGetTangentSamples(TestCase):
    def setUp(self):
        self.spline = PolySpline()
        self.spline.appendPoint((0, 0, 0))
        self.spline.appendPoint((4, 0, 0))
        self.spline.appendPoint((4, 2, 0))
        self.spline.appendPoint((10, 2, 10))

    def testNormal(self):
        samples = self.spline.getTangentSamples(5)
        self.assertEqual(len(samples), 5)
        testEqual(self, samples[0], (4, 0, 0))
        testEqual(self, samples[1], (4, 0, 0))
        testEqual(self, samples[2], (0, 2, 0))
        testEqual(self, samples[3], (6, 0, 10))
        testEqual(self, samples[4], (6, 0, 10))

class TestGetUniformParameters(TestCase):
    def testEmptySpline(self):
        spline = PolySpline()
        parameters = spline.getUniformParameters(10)
        self.assertEqual(len(parameters), 10)

    def testSplineWithLengthZero(self):
        spline = PolySpline()
        for i in range(10):
            spline.appendPoint((3, 4, 5))
        parameters = spline.getUniformParameters(10)
        self.assertEqual(len(parameters), 10)

    def testSimple(self):
        spline = PolySpline()
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((3, 0, 0))
        parameters = spline.getUniformParameters(4)
        self.assertEqual(len(parameters), 4)
        self.assertAlmostEqual(parameters[0], 0.0)
        self.assertAlmostEqual(parameters[1], 1/3)
        self.assertAlmostEqual(parameters[2], 2/3)
        self.assertAlmostEqual(parameters[3], 1.0)

    def testMultiplePointsSameDistance(self):
        spline = PolySpline()
        spline.appendPoint((0, 0, 0))
        spline.appendPoint((3, 0, 0))
        spline.appendPoint((6, 0, 0))
        spline.appendPoint((6, -3, 0))
        spline.appendPoint((6, -3, 3))
        parameters = spline.getUniformParameters(6)
        self.assertAlmostEqual(parameters[0], 0.0)
        self.assertAlmostEqual(parameters[1], 0.2)
        self.assertAlmostEqual(parameters[2], 0.4)
        self.assertAlmostEqual(parameters[3], 0.6)
        self.assertAlmostEqual(parameters[4], 0.8)
        self.assertAlmostEqual(parameters[5], 1.0)

    def testMultiplePointsDifferentDistances(self):
        spline = self.getTestSpline()
        parameters = spline.getUniformParameters(6)
        self.assertAlmostEqual(parameters[0], 0.0)
        self.assertAlmostEqual(parameters[1], 0.3)
        self.assertAlmostEqual(parameters[2], 0.48)
        self.assertAlmostEqual(parameters[3], 0.6)
        self.assertAlmostEqual(parameters[4], 0.8)
        self.assertAlmostEqual(parameters[5], 1.0)

    def testCyclic(self):
        spline = self.getTestSpline()
        spline.cyclic = True
        parameters = spline.getUniformParameters(5)
        self.assertAlmostEqual(parameters[0], 0.0)
        self.assertAlmostEqual(parameters[1], 0.35)
        self.assertAlmostEqual(parameters[2], 0.5)
        self.assertAlmostEqual(parameters[3], 0.75)
        self.assertAlmostEqual(parameters[4], 1.0)

    def getTestSpline(self):
        spline = PolySpline()
        spline.appendPoint((-1, 0, 0))
        spline.appendPoint((1, 0, 0))
        spline.appendPoint((1, 2, 0))
        spline.appendPoint((1, 2, 5))
        spline.appendPoint((-2, 2, 5))
        spline.appendPoint((-2, 2, 2))
        return spline

class TestGetTrimmedCopy(TestCase):
    def testSingleSegment(self):
        spline = PolySpline()
        spline.appendPoint((-1, 0, 0))
        spline.appendPoint((1, 0, 0))
        newSpline = spline.getTrimmedCopy(start = 0.25, end = 0.8)
        self.assertEqual(len(newSpline.points), 2)
        testEqual(self, newSpline.points[0], (-0.5, 0, 0))
        testEqual(self, newSpline.points[1], (0.6, 0, 0))

    def testMultipleSegments(self):
        spline = self.getTestSpline()
        newSpline = spline.getTrimmedCopy(start = 0.25, end = 0.9)
        self.assertEqual(len(newSpline.points), 5)
        testEqual(self, newSpline.points[0], (1, 0, 0.25))
        testEqual(self, newSpline.points[1], (1, 0, 1))
        testEqual(self, newSpline.points[2], (1, -3, 1))
        testEqual(self, newSpline.points[3], (1, -3, 0))
        testEqual(self, newSpline.points[4], (3, -3, 0))

    def testCyclic(self):
        spline = self.getTestSpline()
        spline.cyclic = True
        newSpline = spline.getTrimmedCopy(start = 0.4, end = 0.95)
        self.assertEqual(len(newSpline.points), 5)
        testEqual(self, newSpline.points[0], (1, -1.2, 1))
        testEqual(self, newSpline.points[1], (1, -3, 1))
        testEqual(self, newSpline.points[2], (1, -3, 0))
        testEqual(self, newSpline.points[3], (5, -3, 0))
        testEqual(self, newSpline.points[4], (0.8, -0.9, 0))

    def getTestSpline(self):
        spline = PolySpline()
        spline.appendPoint((-1, 0, 0))
        spline.appendPoint((1, 0, 0))
        spline.appendPoint((1, 0, 1))
        spline.appendPoint((1, -3, 1))
        spline.appendPoint((1, -3, 0))
        spline.appendPoint((5, -3, 0))
        return spline

class TestProject(TestCase):
    def testEmptySpline(self):
        spline = PolySpline()
        with self.assertRaises(Exception):
            spline.project((1, 2, 3))

    def testOnSpline(self):
        spline = self.getTestSpline()
        self.assertAlmostEqual(spline.project((-1, 0, 0)), 0)
        self.assertAlmostEqual(spline.project((0, 0, 0)), 0.25)
        self.assertAlmostEqual(spline.project((1, 0, 0)), 0.5)
        self.assertAlmostEqual(spline.project((1, 0, 2)), 0.75)
        self.assertAlmostEqual(spline.project((1, 0, 4)), 1)

    def testNearSpline(self):
        spline = self.getTestSpline()
        self.assertAlmostEqual(spline.project((0, 0, 0.3)), 0.25)
        self.assertAlmostEqual(spline.project((1, 0.4, 2)), 0.75)

    def testEnd(self):
        spline = self.getTestSpline()
        self.assertAlmostEqual(spline.project((-3, 0.1, 0.2)), 0)
        self.assertAlmostEqual(spline.project((-3, 0.1, 10)), 1)

    def testCyclic(self):
        spline = self.getTestSpline()
        spline.cyclic = True
        self.assertAlmostEqual(spline.project((1, 0, 0)), 1/3)
        self.assertAlmostEqual(spline.project((0, 0, 10)), 2/3)
        self.assertAlmostEqual(spline.project((0, 2, 2)), 5/6)

    def getTestSpline(self):
        spline = PolySpline()
        spline.appendPoint((-1, 0, 0))
        spline.appendPoint((1, 0, 0))
        spline.appendPoint((1, 0, 4))
        return spline

class TestProjectExtended(TestCase):
    def testStart(self):
        spline = self.getTestSpline()
        point, tangent = spline.projectExtended((-5, 1, 2))
        testEqual(self, point, (-5, 0, 0))
        testEqual(self, tangent, (2, 0, 0))

    def testEnd(self):
        spline = self.getTestSpline()
        point, tangent = spline.projectExtended((3, 5, 20))
        testEqual(self, point, (1, 0, 20))
        testEqual(self, tangent, (0, 0, 4))

    def getTestSpline(self):
        spline = PolySpline()
        spline.appendPoint((-1, 0, 0))
        spline.appendPoint((1, 0, 0))
        spline.appendPoint((1, 0, 4))
        return spline


def testEqual(testCase, vector1, vector2):
    testCase.assertAlmostEqual(vector1[0], vector2[0], places = 5)
    testCase.assertAlmostEqual(vector1[1], vector2[1], places = 5)
    testCase.assertAlmostEqual(vector1[2], vector2[2], places = 5)
