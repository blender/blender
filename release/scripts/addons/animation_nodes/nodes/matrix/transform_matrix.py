import bpy
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import multiplyMatrixWithList

class TransformMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformMatrixNode"
    bl_label = "Transform Matrix"

    useMatrixList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Matrix", "useMatrixList",
            ("Matrix", "inMatrix"), ("Matrices", "inMatrices")))

        self.newInput("Matrix", "Transformation", "transformation")

        self.newOutput(VectorizedSocket("Matrix", "useMatrixList",
            ("Matrix", "outMatrix"), ("Matrices", "outMatrices")))

    def getExecutionFunctionName(self):
        if self.useMatrixList:
            return "execute_MatrixList"
        else:
            return "execute_Matrix"

    def execute_Matrix(self, inMatrix, transformation):
        return transformation * inMatrix

    def execute_MatrixList(self, inMatrices, _transformation):
        return multiplyMatrixWithList(inMatrices, _transformation, type = "LEFT")
