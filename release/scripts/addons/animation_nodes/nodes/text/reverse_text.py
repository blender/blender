import bpy
from ... base_types import VectorizedNode

class ReverseTextNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ReverseTextNode"
    bl_label = "Reverse Text"

    useList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Text", "useList",
            ("Text", "inText"), ("Texts", "inTexts"))

        self.newVectorizedOutput("Text", "useList",
            ("Text", "outText"), ("Texts", "outTexts"))

    def getExecutionCode(self):
        if self.useList:
            return "outTexts = [text[::-1] for text in inTexts]"
        else:
            return "outText = inText[::-1]"
