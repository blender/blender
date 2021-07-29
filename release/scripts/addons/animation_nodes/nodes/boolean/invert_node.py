import bpy
from ... base_types import VectorizedNode

class InvertBooleanNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_InvertBooleanNode"
    bl_label = "Invert Boolean"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Boolean", "useList",
            ("Input", "input"),
            ("Input", "input", dict(dataIsModified = True)))

        self.newVectorizedOutput("Boolean", "useList",
            ("Output", "output"),
            ("Output", "output"))

    def getExecutionCode(self):
        if self.useList:
            yield "input.invertAll()"
            yield "output = input"
        else:
            yield "output = not input"
