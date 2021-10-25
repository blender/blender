import bpy
from . c_utils import calculateVectorDistances
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import VirtualVector3DList

class VectorDistanceNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorDistanceNode"
    bl_label = "Vector Distance"

    useListA = VectorizedSocket.newProperty()
    useListB = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useListA",
            ("A", "a"), ("A", "a")))
        self.newInput(VectorizedSocket("Vector", "useListB",
            ("B", "b"), ("B", "b")))

        self.newOutput(VectorizedSocket("Float", ["useListA", "useListB"],
            ("Distance", "distance"), ("Distances", "distances")))

    def getExecutionCode(self, required):
        if self.useListA or self.useListB:
            yield "distances = self.calcDistances(a, b)"
        else:
            yield "distance = (a - b).length"

    def calcDistances(self, a, b):
        vectors1 = VirtualVector3DList.create(a, (0, 0, 0))
        vectors2 = VirtualVector3DList.create(b, (0, 0, 0))
        amount = VirtualVector3DList.getMaxRealLength(vectors1, vectors2)
        return calculateVectorDistances(amount, vectors1, vectors2)
