import bpy
from ... base_types import VectorizedNode
from . c_utils import (extractMatrixTranslations,
                                    extractMatrixRotations,
                                    extractMatrixScales)

class DecomposeMatrixNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_DecomposeMatrixNode"
    bl_label = "Decompose Matrix"

    useMatrixList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Matrix", "useMatrixList",
            ("Matrix", "matrix"), ("Matrices", "matrices"))

        self.newVectorizedOutput("Vector", "useMatrixList",
            ("Translation", "translation"), ("Translations", "translations"))

        self.newVectorizedOutput("Euler", "useMatrixList",
            ("Rotation", "rotation"), ("Rotations", "rotations"))

        self.newVectorizedOutput("Vector", "useMatrixList",
            ("Scale", "scale"), ("Scales", "scales"))


    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if self.useMatrixList:
            if isLinked["translations"]: yield "translations = self.toTranslations(matrices)"
            if isLinked["rotations"]:    yield "rotations = self.toRotations(matrices)"
            if isLinked["scales"]:       yield "scales = self.toScales(matrices)"
        else:
            if isLinked["translation"]: yield "translation = matrix.to_translation()"
            if isLinked["rotation"]:    yield "rotation = matrix.to_euler()"
            if isLinked["scale"]:       yield "scale = matrix.to_scale()"

    def toTranslations(self, matrices):
        return extractMatrixTranslations(matrices)

    def toRotations(self, matrices):
        return extractMatrixRotations(matrices)

    def toScales(self, matrices):
        return extractMatrixScales(matrices)
