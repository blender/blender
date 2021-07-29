import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from . c_utils import getAxisListOfVectorList

class SeparateVectorNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_SeparateVectorNode"
    bl_label = "Separate Vector"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useList",
            ("Vector", "vector"), ("Vectors", "vectors"))

        for axis in "XYZ":
            self.newVectorizedOutput("Float", "useList",
                (axis, axis.lower()), (axis, axis.lower()))

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        for i, axis in enumerate("xyz"):
            if isLinked[axis]:
                if self.useList:
                    yield "{0} = self.getAxisList(vectors, '{0}')".format(axis)
                else:
                    yield "{} = vector[{}]".format(axis, i)

    def getAxisList(self, vectors, axis):
        return getAxisListOfVectorList(vectors, axis)
