import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import VectorizedNode
from ... data_structures import PolySpline, BezierSpline, FloatList, DoubleList

splineTypeItems = [
    ("BEZIER", "Bezier", "Each control point has two handles", "CURVE_BEZCURVE", 0),
    ("POLY", "Poly", "Linear interpolation between the spline points", "NOCURVE", 1)
]

class SplineFromPointsNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_SplineFromPointsNode"
    bl_label = "Spline from Points"

    splineType = EnumProperty(name = "Spline Type", default = "BEZIER",
        items = splineTypeItems, update = VectorizedNode.refresh)

    useRadiusList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newInput("Vector List", "Points", "points", dataIsModified = True)
        if self.splineType == "BEZIER":
            self.newInput("Vector List", "Left Handles", "leftHandles", dataIsModified = True)
            self.newInput("Vector List", "Right Handles", "rightHandles", dataIsModified = True)
        self.newVectorizedInput("Float", "useRadiusList",
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii", dict(dataIsModified = True)))
        self.newInput("Boolean", "Cyclic", "cyclic", value = False)
        self.newOutput("Spline", "Spline", "spline")

    def draw(self, layout):
        layout.prop(self, "splineType", text = "")

    def getExecutionFunctionName(self):
        if self.splineType == "BEZIER":
            return "execute_Bezier"
        elif self.splineType == "POLY":
            return "execute_Poly"

    def execute_Bezier(self, points, leftHandles, rightHandles, radii, cyclic):
        self.correctHandlesListIfNecessary(points, leftHandles)
        self.correctHandlesListIfNecessary(points, rightHandles)
        radii = self.prepareRadiusList(radii, len(points))
        return BezierSpline(points, leftHandles, rightHandles, radii, cyclic)

    def correctHandlesListIfNecessary(self, points, handles):
        if len(points) < len(handles):
            del handles[len(points):]
        elif len(points) > len(handles):
            handles += points[len(handles):]

    def execute_Poly(self, points, radii, cyclic):
        radii = self.prepareRadiusList(radii, len(points))
        return PolySpline(points, radii, cyclic)

    def prepareRadiusList(self, radii, pointAmount):
        if isinstance(radii, DoubleList):
            radii = FloatList.fromValues(radii)

        if not isinstance(radii, FloatList):
            radii = FloatList.fromValues([radii]) * pointAmount

        if pointAmount > len(radii):
            radii.extend(FloatList.fromValues([0]) * (pointAmount - len(radii)))
        elif pointAmount < len(radii):
            del radii[pointAmount:]

        return radii
