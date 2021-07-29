import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from . c_utils import createAxisRotations, createRotationsFromEulers

axisItems = [
    ("X", "X", "", "", 0),
    ("Y", "Y", "", "", 1),
    ("Z", "Z", "", "", 2),
    ("ALL", "All", "", "", 3) ]

class RotationMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_RotationMatrixNode"
    bl_label = "Rotation Matrix"

    axis = EnumProperty(default = "X", items = axisItems,
        update = VectorizedNode.refresh)

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        socketType = "Euler" if self.axis == "ALL" else "Float"
        self.newVectorizedInput(socketType, "useList",
            ("Angle", "angle"), ("Angles", "angles"))

        self.newVectorizedOutput("Matrix", "useList",
            ("Matrix", "matrix"), ("Matrices", "matrices"))

    def draw(self, layout):
        layout.prop(self, "axis", expand = True)

    def getExecutionFunctionName(self):
        if self.useList:
            if self.axis == "ALL":
                return "execute_List_All"
            else:
                return "execute_List_Axis"

    def getExecutionCode(self):
        if len(self.axis) == 1:
            return "matrix = Matrix.Rotation(angle, 4, '{}')".format(self.axis)
        else:
            return "matrix = angle.to_matrix(); matrix.resize_4x4()"

    def execute_List_All(self, rotations):
        return createRotationsFromEulers(rotations)

    def execute_List_Axis(self, angles):
        return createAxisRotations(angles, self.axis)
