import bpy
from ... base_types import VectorizedNode
from . c_utils import createTranslationMatrices

class TranslationMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TranslationMatrixNode"
    bl_label = "Translation Matrix"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useList",
            ("Translation", "translation"), ("Translations", "translations"))

        self.newVectorizedOutput("Matrix", "useList",
            ("Matrix", "matrix"), ("Matrices", "matrices"))

    def getExecutionCode(self):
        if self.useList:
            return "matrices = self.calcMatrices(translations)"
        else:
            return "matrix = Matrix.Translation(translation)"

    def calcMatrices(self, vectors):
        return createTranslationMatrices(vectors)
