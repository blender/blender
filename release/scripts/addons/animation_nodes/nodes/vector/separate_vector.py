import bpy
from bpy.props import *
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import getAxisListOfVectorList

class SeparateVectorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparateVectorNode"
    bl_label = "Separate Vector"

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useList",
            ("Vector", "vector"), ("Vectors", "vectors")))

        for axis in "XYZ":
            self.newOutput(VectorizedSocket("Float", "useList",
                (axis, axis.lower()), (axis, axis.lower())))

    def getExecutionCode(self, required):
        for i, axis in enumerate("xyz"):
            if axis in required:
                if self.useList:
                    yield "{0} = self.getAxisList(vectors, '{0}')".format(axis)
                else:
                    yield "{} = vector[{}]".format(axis, i)

    def getAxisList(self, vectors, axis):
        return getAxisListOfVectorList(vectors, axis)
