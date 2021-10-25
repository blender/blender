import bpy
from . c_utils import vectorsFromValues
from ... base_types import AnimationNode, VectorizedSocket

class VectorFromValueNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorFromValueNode"
    bl_label = "Vector from Value"

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Float", "useList",
            ("Value", "value"), ("Values", "values")))

        self.newOutput(VectorizedSocket("Vector", "useList",
            ("Vector", "vector"), ("Vectors", "vectors")))

    def getExecutionCode(self, required):
        if self.useList:
            return "vectors = self.vectorsFromValues(values)"
        else:
            return "vector = Vector((value, value, value))"

    def vectorsFromValues(self, values):
        return vectorsFromValues(values)
