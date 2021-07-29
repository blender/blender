import bpy
from bpy.props import *
from ... base_types import VectorizedNode

class VectorFromValueNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_VectorFromValueNode"
    bl_label = "Vector from Value"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Float", "useList",
            ("Value", "value"), ("Values", "values"))

        self.newVectorizedOutput("Vector", "useList",
            ("Vector", "vector"), ("Vectors", "vectors"))

    def getExecutionCode(self):
        if self.useList:
            return "vectors = AN.nodes.vector.c_utils.vectorsFromValues(values)"
        else:
            return "vector = Vector((value, value, value))"
