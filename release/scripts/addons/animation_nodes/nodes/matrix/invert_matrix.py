import bpy
from ... base_types import AnimationNode

class InvertMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InvertMatrixNode"
    bl_label = "Invert Matrix"

    def create(self):
        self.newInput("Matrix", "Matrix", "matrix")
        self.newOutput("Matrix", "Inverted Matrix", "invertedMatrix")

    def draw(self, layout):
        layout.separator()

    def getExecutionCode(self):
        return "invertedMatrix = matrix.inverted(Matrix.Identity(4))"
