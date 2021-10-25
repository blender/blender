import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, DoubleList

splineTypeItems = [
    ("BEZIER", "Bezier", "Each control point has two handles", "CURVE_BEZCURVE", 0),
    ("POLY", "Poly", "Linear interpolation between the spline points", "NOCURVE", 1)
]

class SplineInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SplineInfoNode"
    bl_label = "Spline Info"

    splineType = EnumProperty(name = "Spline Type", default = "POLY",
        items = splineTypeItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Spline", "Spline", "spline", defaultDrawType = "PROPERTY_ONLY", dataIsModified = True)
        self.newOutput("Vector List", "Points", "points")
        if self.splineType == "BEZIER":
            self.newOutput("Vector List", "Left Handles", "leftHandles")
            self.newOutput("Vector List", "Right Handles", "rightHandles")
        self.newOutput("Float List", "Radii", "radii")
        self.newOutput("Float List", "Tilts", "tilts")
        self.newOutput("Boolean", "Cyclic", "cyclic")
        self.newOutput("Integer", "Point Amount", "pointAmount")

    def draw(self, layout):
        layout.prop(self, "splineType", text = "")

    def getExecutionCode(self, required):
        if "points" in required:
            yield "points = spline.points"
        if "radii" in required:
            yield "radii = DoubleList.fromValues(spline.radii)"
        if "tilts" in required:
            yield "tilts = DoubleList.fromValues(spline.tilts)"
        if "cyclic" in required:
            yield "cyclic = spline.cyclic"
        if "pointAmount" in required:
            yield "pointAmount = len(spline.points)"

        if self.splineType == "BEZIER":
            if "leftHandles" in required:
                yield "leftHandles = spline.leftHandles if spline.type == 'BEZIER' else Vector3DList()"
            if "rightHandles" in required:
                yield "rightHandles = spline.rightHandles if spline.type == 'BEZIER' else Vector3DList()"
