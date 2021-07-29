import bpy
from ... base_types import VectorizedNode
from . c_utils import multiplyMatrixWithList

class TransformMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TransformMatrixNode"
    bl_label = "Transform Matrix"

    useMatrixList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Matrix", "useMatrixList",
            ("Matrix", "inMatrix"), ("Matrices", "inMatrices"))

        self.newInput("Matrix", "Transformation", "transformation")

        self.newVectorizedOutput("Matrix", "useMatrixList",
            ("Matrix", "outMatrix"), ("Matrices", "outMatrices"))

    def getExecutionFunctionName(self):
        if self.useMatrixList:
            return "execute_MatrixList"
        else:
            return "execute_Matrix"

    def execute_Matrix(self, inMatrix, transformation):
        return transformation * inMatrix

    def execute_MatrixList(self, inMatrices, _transformation):
        return multiplyMatrixWithList(inMatrices, _transformation, type = "LEFT")
