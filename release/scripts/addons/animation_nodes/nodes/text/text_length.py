import bpy
from ... base_types import AnimationNode, VectorizedSocket

class TextLengthNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TextLengthNode"
    bl_label = "Text Length"
    codeEffects = [VectorizedSocket.CodeEffect]

    useTextList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Text", "useTextList",
            ("Text", "text"), ("Texts", "texts")))

        self.newOutput(VectorizedSocket("Integer", "useTextList",
            ("Length", "length"), ("Lengths", "lengths")))

    def getExecutionCode(self, required):
        return "length = len(text)"
