import bpy
from ... base_types import AnimationNode, VectorizedSocket

class TransformVectorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformVectorNode"
    bl_label = "Transform Vector"

    useVectorList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useVectorList",
            ("Vector", "vector"),
            ("Vectors", "vectors", dict(dataIsModified = True))))

        self.newInput("Matrix", "Matrix", "matrix")

        self.newOutput(VectorizedSocket("Vector", "useVectorList",
            ("Vector", "transformedVector"), ("Vectors", "vectors")))

    def getExecutionCode(self, required):
        if self.useVectorList:
            return "vectors.transform(matrix)"
        else:
            return "transformedVector = matrix * vector"
