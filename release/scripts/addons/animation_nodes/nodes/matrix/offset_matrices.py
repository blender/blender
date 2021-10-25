import bpy
from bpy.props import *
from ... base_types import AnimationNode
from . transformation_base_node import MatrixTransformationBase

class OffsetMatrixNode(bpy.types.Node, AnimationNode, MatrixTransformationBase):
    bl_idname = "an_OffsetMatrixNode"
    bl_label = "Offset Matrix"
    bl_width_default = 200
    onlySearchTags = True
    errorHandlingType = "EXCEPTION"
    searchTags = [("Offset Matrices", {"useMatrixList" : repr(True)})]

    useMatrixList = BoolProperty(name = "Use Matrix List", default = False,
        update = AnimationNode.refresh)

    def create(self):
        if self.useMatrixList:
            self.newInput("Matrix List", "Matrices", "inMatrices",
                dataIsModified = self.transformsOriginalMatrixList)
            self.newOutput("Matrix List", "Matrices", "outMatrices")
        else:
            self.newInput("Matrix", "Matrix", "inMatrix")
            self.newOutput("Matrix", "Matrix", "outMatrix")

        self.createMatrixTransformationInputs(self.useMatrixList)

    def draw(self, layout):
        self.draw_MatrixTransformationProperties(layout)

    def drawAdvanced(self, layout):
        layout.prop(self, "useMatrixList")
        self.drawAdvanced_MatrixTransformationProperties(layout)

    def getExecutionFunctionName(self):
        return self.getMatrixTransformationFunctionName(self.useMatrixList)
