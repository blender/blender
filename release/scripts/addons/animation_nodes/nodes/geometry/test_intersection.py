from mathutils import Vector
from unittest import TestCase
from . c_utils import (
    intersect_LineLine_Single,
    intersect_LinePlane_Single,
    intersect_LineSphere_Single,
    intersect_PlanePlane_Single,
    intersect_SpherePlane_Single,
    intersect_SphereSphere_Single)

class TestLineLine(TestCase):
    def testValidLineLine2DInBound(self):
        self.result = intersect_LineLine_Single(Vector((1, 1, 0)),
                                                Vector((-1, -1, 0)),
                                                Vector((1, -1, 0)),
                                                Vector((-1, 1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0.5, 0.5, True))

    def testValidLineLine2DOutBound(self):
        self.result = intersect_LineLine_Single(Vector((-1, -1, 0)),
                                                Vector((-3, -3, 0)),
                                                Vector((1, -1, 0)),
                                                Vector((-1, 1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       -0.5, 0.5, True))

    def testValidLineLine3DInBound(self):
        self.result = intersect_LineLine_Single(Vector((1, 1, 1)),
                                                Vector((-1, -1, 1)),
                                                Vector((1, -1, 0)),
                                                Vector((-1, 1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 1)),
                                       Vector((0, 0, 0)),
                                       0.5, 0.5, True))

    def testValidLineLine2DOutBound(self):
        self.result = intersect_LineLine_Single(Vector((-1, -1, 1)),
                                                Vector((-3, -3, 1)),
                                                Vector((1, -1, 0)),
                                                Vector((-1, 1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 1)),
                                       Vector((0, 0, 0)),
                                       -0.5, 0.5, True))

    def testInvalidLineLine2D(self):
        self.result = intersect_LineLine_Single(Vector((1, 1, 0)),
                                                Vector((1, -1, 0)),
                                                Vector((-1, 1, 0)),
                                                Vector((-1, -1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0, 0, False))

    def testInvalidLineLine2DLine(self):
        self.result = intersect_LineLine_Single(Vector((1, 1, 0)),
                                                Vector((1, -1, 0)),
                                                Vector((1, 1, 0)),
                                                Vector((1, -1, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0, 0, False))

    def testInvalidLineLine3D(self):
        self.result = intersect_LineLine_Single(Vector((2, -1, 1)),
                                                Vector((0, 1, 1)),
                                                Vector((1, -2, 0)),
                                                Vector((-1, 0, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0, 0, False))

class TestLinePlane(TestCase):
    def testValidLinePlanePositiveInBound(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 1)),
                                                 Vector((0, 0, -1)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0.5, True))

    def testValidLinePlaneNegativeInBound(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 1)),
                                                 Vector((0, 0, -1)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, -1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0.5, True))

    def testValidLinePlanePsoitiveOutBound(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 1)),
                                                 Vector((0, 0, 2)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), -1, True))

    def testValidLinePlaneNegativeOutBound(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 1)),
                                                 Vector((0, 0, 2)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, -1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), -1, True))

    def testInvalidLinePlane(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 1)),
                                                 Vector((1, 0, 1)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0, False))

    def testInvalidLinePlaneOnPlane(self):
        self.result = intersect_LinePlane_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((0, 0, 0)),
                                                 Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0, False))

class TestLineSphere(TestCase):
    def testLineSphereOneIntersectionInBound(self):
        self.result = intersect_LineSphere_Single(Vector((1, 0, 1)),
                                                  Vector((-1, 0, 1)),
                                                  Vector((0, 0, 0)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 1)),
                                       Vector((0, 0, 1)),
                                       0.5, 0.5, 1))

    def testLineSphereOneIntersectionOutBound(self):
        self.result = intersect_LineSphere_Single(Vector((1, 0, 1)),
                                                  Vector((2, 0, 1)),
                                                  Vector((0, 0, 0)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 1)),
                                       Vector((0, 0, 1)),
                                       -1, -1, 1))

    def testLineSphereOneIntersectionZeroRadius(self):
        self.result = intersect_LineSphere_Single(Vector((0, 0, -1)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 0)), 0)
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0.5, 0.5, 1))

    def testLineSphereTwoIntersectionsInBound(self):
       self.result = intersect_LineSphere_Single(Vector((0, 0, 0)),
                                                 Vector((0, 0, 2)),
                                                 Vector((0, 0, 0)), 1)
       self.assertEqual(self.result, (Vector((0, 0, 1)),
                                      Vector((0, 0, -1)),
                                      0.5, -0.5, 2))

    def testLineSphereTwoIntersectionsOutBound(self):
       self.result = intersect_LineSphere_Single(Vector((0, 0, 2)),
                                                 Vector((0, 0, 3)),
                                                 Vector((0, 0, 0)), 1)
       self.assertEqual(self.result, (Vector((0, 0, 1)),
                                      Vector((0, 0, -1)),
                                      -1, -3, 2))

    def testInvalidLineSphere(self):
        self.result = intersect_LineSphere_Single(Vector((2, 0, -1)),
                                                  Vector((2, 0, 1)),
                                                  Vector((0, 0, 0)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 0)),
                                       Vector((0, 0, 0)),
                                       0, 0, 0))

class TestPlanePlane(TestCase):
    def testValidPlanePlane(self):
        self.result = intersect_PlanePlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 0)),
                                                  Vector((1, 0, 0)))
        self.assertEqual(self.result, (Vector((0, 1, 0)), Vector((0, 0, 0)), True))

    def testValidPlanePlaneShifted(self):
        self.result = intersect_PlanePlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 2, 0)),
                                                  Vector((1, 0, 0)))
        self.assertEqual(self.result, (Vector((0, 1, 0)), Vector((0, 0, 0)), True))

    def testInvalidPlanePlane(self):
        self.result = intersect_PlanePlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), False))

    def testInvalidPlanePlaneOverlaped(self):
        self.result = intersect_PlanePlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), False))

class TestSpherePlane(TestCase):
    def testValidSpherePlane(self):
        self.result = intersect_SpherePlane_Single(Vector((0, 0, 0)), 1,
                                                   Vector((0, 0, 0)),
                                                   Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 1, True))

    def testValidSpherePlaneShifted(self):
        self.result = intersect_SpherePlane_Single(Vector((0, 0, 0)), 1,
                                                   Vector((2, 2, 0)),
                                                   Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 1, True))

    def testValidSpherePlaneZeroRadius(self):
        self.result = intersect_SpherePlane_Single(Vector((0, 0, 0)), 0,
                                                   Vector((0, 0, 0)),
                                                   Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0, True))

    def testInvalidSpherePlane(self):
        self.result = intersect_SpherePlane_Single(Vector((0, 0, 0)), 1,
                                                   Vector((0, 0, 9)),
                                                   Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0, False))

class TestSphereSphere(TestCase):
    def testValidSphereSphereTouching(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, -1)), 1,
                                                    Vector((0, 0, 1)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 2)), 0, True))

    def testInvalidSphereSphere(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, -2)), 1,
                                                    Vector((0, 0, 2)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), 0, False))

    def testInvalidSphereSphereInside(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, 0)), 1,
                                                    Vector((0, 0, 0)), 0.5)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), 0, False))

    def testInvalidSphereSphereOutside(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, 0)), 1,
                                                    Vector((0, 0, 0)), 2)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), 0, False))

    def testInvalidSphereSphereConcentric(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, 0)), 1,
                                                    Vector((0, 0, 0)), 1)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), 0, False))

    def testInvalidSphereSphereZeroRadius(self):
        self.result = intersect_SphereSphere_Single(Vector((0, 0, 0)), 0,
                                                    Vector((0, 0, 0)), 0)
        self.assertEqual(self.result, (Vector((0, 0, 0)), Vector((0, 0, 0)), 0, False))
