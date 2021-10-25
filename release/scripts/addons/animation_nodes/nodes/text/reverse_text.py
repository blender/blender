import bpy
from ... base_types import AnimationNode, VectorizedSocket

class ReverseTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReverseTextNode"
    bl_label = "Reverse Text"

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Text", "useList",
            ("Text", "inText"), ("Texts", "inTexts")))

        self.newOutput(VectorizedSocket("Text", "useList",
            ("Text", "outText"), ("Texts", "outTexts")))

    def getExecutionCode(self, required):
        if self.useList:
            return "outTexts = [text[::-1] for text in inTexts]"
        else:
            return "outText = inText[::-1]"
