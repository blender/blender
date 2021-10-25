import bpy
from bpy.props import *
from ... events import propertyChanged
from ... data_structures import PolySpline, BezierSpline
from ... base_types import AnimationNode, VectorizedSocket

targetTypeItems = [
    ("BEZIER", "Bezier", "Each control point has two handles", "CURVE_BEZCURVE", 0),
    ("POLY", "Poly", "Linear interpolation between the spline points", "NOCURVE", 1)
]

class ChangeSplineTypeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ChangeSplineTypeNode"
    bl_label = "Change Spline Type"
    codeEffects = [VectorizedSocket.CodeEffect]

    useSplineList = VectorizedSocket.newProperty()

    targetType = EnumProperty(name = "Target Type", default = "POLY",
        items = targetTypeItems, update = propertyChanged)

    def create(self):
        socket = self.newInput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "inSpline"), ("Splines", "inSplines")))
        socket.defaultDrawType = "PROPERTY_ONLY"

        self.newOutput(VectorizedSocket("Spline", "useSplineList",
            ("Spline", "outSpline"), ("Splines", "outSplines")))

    def draw(self, layout):
        layout.prop(self, "targetType", text = "")

    def getExecutionCode(self, required):
        return "outSpline = self.convertToTargetType(inSpline)"

    def convertToTargetType(self, spline):
        if self.targetType == "POLY":
            if spline.type == "POLY":
                return spline.copy()
            elif spline.type == "BEZIER":
                return PolySpline(points = spline.points.copy(),
                                  radii = spline.radii.copy(),
                                  tilts = spline.tilts.copy(),
                                  cyclic = spline.cyclic)
        elif self.targetType == "BEZIER":
            if spline.type == "BEZIER":
                return spline.copy()
            elif spline.type == "POLY":
                return BezierSpline(points = spline.points.copy(),
                                    radii = spline.radii.copy(),
                                    tilts = spline.tilts.copy(),
                                    cyclic = spline.cyclic)
