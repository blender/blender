import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from . c_utils import (
    replicateMatrixAtMatrices,
    replicateMatrixAtVectors,
    replicateMatricesAtMatrices,
    replicateMatricesAtVectors
)

transformationTypeItems = [
    ("Matrix List", "Matrices", "", "NONE", 0),
    ("Vector List", "Vectors", "", "NONE", 1)
]

class ReplicateMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ReplicateMatrixNode"
    bl_label = "Replicate Matrix"

    useMatrixList = VectorizedNode.newVectorizeProperty()

    transformationType = EnumProperty(name = "Transformation Type", default = "Matrix List",
        items = transformationTypeItems, update = VectorizedNode.refresh)

    def create(self):
        self.newVectorizedInput("Matrix", "useMatrixList",
            ("Matrix", "inMatrix"), ("Matrices", "inMatrices"))

        self.newInput(self.transformationType, "Transformations", "transformations")

        self.newOutput("Matrix List", "Matrices", "outMatrices")

    def draw(self, layout):
        layout.prop(self, "transformationType", text = "")

    def getExecutionFunctionName(self):
        if self.transformationType == "Matrix List":
            if self.useMatrixList:
                return "execute_List_Matrices"
            else:
                return "execute_Single_Matrices"
        elif self.transformationType == "Vector List":
            if self.useMatrixList:
                return "execute_List_Vectors"
            else:
                return "execute_Single_Vectors"

    def execute_Single_Matrices(self, matrix, transformations):
        return replicateMatrixAtMatrices(matrix, transformations)

    def execute_Single_Vectors(self, matrix, translations):
        return replicateMatrixAtVectors(matrix, translations)

    def execute_List_Matrices(self, matrices, transformations):
        return replicateMatricesAtMatrices(matrices, transformations)

    def execute_List_Vectors(self, matrices, translations):
        return replicateMatricesAtVectors(matrices, translations)
