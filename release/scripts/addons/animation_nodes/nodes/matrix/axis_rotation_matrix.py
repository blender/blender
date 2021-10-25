import bpy
from bpy.props import *
from . c_utils import createAxisRotations
from ... events import executionCodeChanged
from ... base_types import AnimationNode, VectorizedSocket

axisItems = [
    ("X", "X", "", "", 0),
    ("Y", "Y", "", "", 1),
    ("Z", "Z", "", "", 2)
]

class AxisRotationMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_AxisRotationMatrixNode"
    bl_label = "Axis Rotation Matrix"

    axis = EnumProperty(default = "X", items = axisItems,
        update = AnimationNode.refresh)

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Float", "useList",
            ("Angle", "angle"), ("Angles", "angles")))

        self.newOutput(VectorizedSocket("Matrix", "useList",
            ("Matrix", "matrix"), ("Matrices", "matrices")))

    def draw(self, layout):
        col = layout.column()
        col.row().prop(self, "axis", expand = True)
        col.prop(self, "useDegree")

    def getExecutionCode(self, required):
        if self.useList:
            yield "matrices = self.execute_List(angles)"
        else:
            if self.useDegree:
                yield "matrix = Matrix.Rotation(angle / 180 * math.pi, 4, '{}')".format(self.axis)
            else:
                yield "matrix = Matrix.Rotation(angle, 4, '{}')".format(self.axis)

    def execute_List(self, angles):
        return createAxisRotations(angles, self.axis, self.useDegree)

    def getUsedModules(self):
        return ["math"]
