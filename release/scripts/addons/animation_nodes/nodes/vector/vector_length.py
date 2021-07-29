import bpy
from ... base_types import VectorizedNode
from . c_utils import calculateVectorLengths

class VectorLengthNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_VectorLengthNode"
    bl_label = "Vector Length"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useList",
            ("Vector", "vector"), ("Vectors", "vectors"))

        self.newVectorizedOutput("Float", "useList",
            ("Length", "length"), ("Lenghts", "lengths"))

    def getExecutionCode(self):
        if self.useList:
            yield "lengths = self.calcLengths(vectors)"
        else:
            yield "length = vector.length"

    def calcLengths(self, vectors):
        return calculateVectorLengths(vectors)
