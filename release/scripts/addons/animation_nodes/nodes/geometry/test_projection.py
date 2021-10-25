from mathutils import Vector
from unittest import TestCase
from . c_utils import (
    project_PointOnLine_Single,
    project_PointOnPlane_Single)

class testProjectOnLine(TestCase):
    def testProjectOnLine(self):
        self.result = project_PointOnLine_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((1, 1, 0)))
        self.assertEqual(self.result, (Vector((1, 0, 0)), 1, 1))

    def testProjectOnLineOnLineStart(self):
        self.result = project_PointOnLine_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((0, 0, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0, 0))

    def testProjectOnLineOnLineEnd(self):
        self.result = project_PointOnLine_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((1, 0, 0)))
        self.assertEqual(self.result, (Vector((1, 0, 0)), 1, 0))

    def testProjectOnLineHalfWay(self):
        self.result = project_PointOnLine_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((0.5, 0, 0)))
        self.assertEqual(self.result, (Vector((0.5, 0, 0)), 0.5, 0))

    def testProjectOnLineOutBound(self):
        self.result = project_PointOnLine_Single(Vector((0, 0, 0)),
                                                 Vector((1, 0, 0)),
                                                 Vector((2, 0, 0)))
        self.assertEqual(self.result, (Vector((2, 0, 0)), 2, 0))

class testProjectOnPlane(TestCase):
    def testProjectOnPlanePositiveDistance(self):
        self.result = project_PointOnPlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 1))

    def testProjectOnPlaneNegativeDistance(self):
        self.result = project_PointOnPlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, -1)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), -1))

    def testProjectOnPlaneOnPlane(self):
        self.result = project_PointOnPlane_Single(Vector((0, 0, 0)),
                                                  Vector((0, 0, 1)),
                                                  Vector((0, 0, 0)))
        self.assertEqual(self.result, (Vector((0, 0, 0)), 0))
