import bpy
from ... base_types import AnimationNode

class VectorDotProductNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorDotProductNode"
    bl_label = "Vector Dot Product"

    def create(self):
        self.newInput("Vector", "A", "a")
        self.newInput("Vector", "B", "b")
        self.newOutput("Float", "Dot Product", "dotProduct")

    def getExecutionCode(self):
        return "dotProduct = a.dot(b)"
