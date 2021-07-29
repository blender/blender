import bpy
from . c_utils import calculateVectorDistances
from ... base_types import VectorizedNode

class VectorDistanceNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_VectorDistanceNode"
    bl_label = "Vector Distance"

    useListA = VectorizedNode.newVectorizeProperty()
    useListB = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useListA",
            ("A", "a"), ("A", "a"))
        self.newVectorizedInput("Vector", "useListB",
            ("B", "b"), ("B", "b"))

        self.newVectorizedOutput("Float", [("useListA", "useListB")],
            ("Distance", "distance"), ("Distances", "distances"))

    def getExecutionCode(self):
        if self.useListA or self.useListB:
            yield "distances = self.calcDistances(a, b)"
        else:
            yield "distance = (a - b).length"

    def calcDistances(self, a, b):
        return calculateVectorDistances(a, b)
