import bpy
from ... base_types import AnimationNode, VectorizedSocket

class ReplaceTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReplaceTextNode"
    bl_label = "Replace Text"
    codeEffects = [VectorizedSocket.CodeEffect]

    useTextList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Text", "useTextList",
            ("Text", "text"), ("Texts", "texts")))
        self.newInput("Text", "Old", "old")
        self.newInput("Text", "New", "new")

        self.newOutput(VectorizedSocket("Text", "useTextList",
            ("Text", "newText"), ("Texts", "newTexts")))

    def getExecutionCode(self, required):
        return "newText = text.replace(old, new)"
