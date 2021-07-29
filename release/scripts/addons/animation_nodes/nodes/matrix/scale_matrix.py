import bpy
from ... base_types import AnimationNode

class ScaleMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ScaleMatrixNode"
    bl_label = "Scale Matrix"

    def create(self):
        self.newInput("Vector", "Scale", "scale", value = [1, 1, 1])
        self.newOutput("Matrix", "Matrix", "matrix")

    def getExecutionCode(self):
        return ("matrix = animation_nodes.utils.math.scaleMatrix(scale)")
