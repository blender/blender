import bpy
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import (
    extractMatrixTranslations,
    extractMatrixRotations,
    extractMatrixScales
)

class DecomposeMatrixNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DecomposeMatrixNode"
    bl_label = "Decompose Matrix"

    useMatrixList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Matrix", "useMatrixList",
            ("Matrix", "matrix"), ("Matrices", "matrices")))

        self.newOutput(VectorizedSocket("Vector", "useMatrixList",
            ("Translation", "translation"), ("Translations", "translations")))

        self.newOutput(VectorizedSocket("Euler", "useMatrixList",
            ("Rotation", "rotation"), ("Rotations", "rotations")))

        self.newOutput(VectorizedSocket("Vector", "useMatrixList",
            ("Scale", "scale"), ("Scales", "scales")))

    def getExecutionCode(self, required):
        if self.useMatrixList:
            if "translations" in required: yield "translations = self.toTranslations(matrices)"
            if "rotations" in required:    yield "rotations = self.toRotations(matrices)"
            if "scales" in required:       yield "scales = self.toScales(matrices)"
        else:
            if "translation" in required: yield "translation = matrix.to_translation()"
            if "rotation" in required:    yield "rotation = matrix.to_euler()"
            if "scale" in required:       yield "scale = matrix.to_scale()"

    def toTranslations(self, matrices):
        return extractMatrixTranslations(matrices)

    def toRotations(self, matrices):
        return extractMatrixRotations(matrices)

    def toScales(self, matrices):
        return extractMatrixScales(matrices)
