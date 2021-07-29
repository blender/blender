import bpy
from bpy.props import *
from ... base_types import VectorizedNode

class ComposeMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ComposeMatrixNode"
    bl_label = "Compose Matrix"

    useTranslationList = VectorizedNode.newVectorizeProperty()
    useRotationList = VectorizedNode.newVectorizeProperty()
    useScaleList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Vector", "useTranslationList",
            ("Translation", "translation"), ("Translations", "translations"))

        self.newVectorizedInput("Euler", "useRotationList",
            ("Rotation", "rotation"), ("Rotations", "rotations"))

        self.newVectorizedInput("Vector", "useScaleList",
            ("Scale", "scale", dict(value = (1, 1, 1))),
            ("Scales", "scales"))

        self.newVectorizedOutput("Matrix", [("useTranslationList", "useRotationList", "useScaleList")],
            ("Matrix", "matrix"), ("Matrices", "matrices"))

    def getExecutionFunctionName(self):
        if self.useTranslationList or self.useRotationList or self.useScaleList:
            return "execute_List"

    def getExecutionCode(self):
        yield "matrix = animation_nodes.utils.math.composeMatrix(translation, rotation, scale)"

    def execute_List(self, translations, rotations, scales):
        from . c_utils import composeMatrices
        return composeMatrices(translations, rotations, scales)
