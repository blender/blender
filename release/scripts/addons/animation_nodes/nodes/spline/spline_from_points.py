import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import PolySpline, BezierSpline, FloatList, DoubleList

splineTypeItems = [
    ("BEZIER", "Bezier", "Each control point has two handles", "CURVE_BEZCURVE", 0),
    ("POLY", "Poly", "Linear interpolation between the spline points", "NOCURVE", 1)
]

class SplineFromPointsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SplineFromPointsNode"
    bl_label = "Spline from Points"

    splineType = EnumProperty(name = "Spline Type", default = "BEZIER",
        items = splineTypeItems, update = AnimationNode.refresh)

    improveBezierHandles = BoolProperty(name = "Improve Bezier Handles",
        description = "Tries to avoid that the handles are equal to the corresponding points.",
        default = True, update = propertyChanged)

    useRadiusList = VectorizedSocket.newProperty()
    useTiltList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("Vector List", "Points", "points", dataIsModified = True)
        if self.splineType == "BEZIER":
            self.newInput("Vector List", "Left Handles", "leftHandles", dataIsModified = True)
            self.newInput("Vector List", "Right Handles", "rightHandles", dataIsModified = True)
        self.newInput(VectorizedSocket("Float", "useRadiusList",
            ("Radius", "radius", dict(value = 0.1, minValue = 0)),
            ("Radii", "radii")))
        self.newInput(VectorizedSocket("Float", "useTiltList",
            ("Tilt", "tilt"),
            ("Tilts", "tilts")))
        self.newInput("Boolean", "Cyclic", "cyclic", value = False)
        self.newOutput("Spline", "Spline", "spline")

    def draw(self, layout):
        layout.prop(self, "splineType", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "improveBezierHandles")

    def getExecutionFunctionName(self):
        if self.splineType == "BEZIER":
            return "execute_Bezier"
        elif self.splineType == "POLY":
            return "execute_Poly"

    def execute_Bezier(self, points, leftHandles, rightHandles, radii, tilts, cyclic):
        self.correctHandlesListIfNecessary(points, leftHandles)
        self.correctHandlesListIfNecessary(points, rightHandles)
        _radii = self.prepareFloatList(radii, len(points))
        _tilts = self.prepareFloatList(tilts, len(points))

        spline = BezierSpline(points, leftHandles, rightHandles, _radii, _tilts, cyclic)
        if self.improveBezierHandles:
            spline.improveStraightBezierSegments()

        return spline

    def correctHandlesListIfNecessary(self, points, handles):
        if len(points) < len(handles):
            del handles[len(points):]
        elif len(points) > len(handles):
            handles += points[len(handles):]

    def execute_Poly(self, points, radii, tilts, cyclic):
        _radii = self.prepareFloatList(radii, len(points))
        _tilts = self.prepareFloatList(tilts, len(points))
        return PolySpline(points, _radii, _tilts, cyclic)

    def prepareFloatList(self, source, pointAmount):
        if isinstance(source, DoubleList):
            return FloatList.fromValues(source).repeated(length = pointAmount, default = 0)
        else:
            return FloatList.fromValue(source, length = pointAmount)
