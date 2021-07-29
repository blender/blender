import bpy
from bpy.props import *
from ... base_types import VectorizedNode

class TransformVectorNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TransformVectorNode"
    bl_label = "Transform Vector"

    useVectorList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useVectorList",
            ("Vector", "vector"),
            ("Vectors", "vectors", dict(dataIsModified = True)))

        self.newInput("Matrix", "Matrix", "matrix")

        self.newVectorizedOutput("Vector", "useVectorList",
            ("Vector", "transformedVector"), ("Vectors", "vectors"))

    def getExecutionCode(self):
        if self.useVectorList:
            return "vectors.transform(matrix)"
        else:
            return "transformedVector = matrix * vector"
