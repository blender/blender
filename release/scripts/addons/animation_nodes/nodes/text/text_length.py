import bpy
from ... base_types import VectorizedNode

class TextLengthNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_TextLengthNode"
    bl_label = "Text Length"
    autoVectorizeExecution = True

    useTextList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Text", "useTextList",
            ("Text", "text"), ("Texts", "texts"))

        self.newVectorizedOutput("Integer", "useTextList",
            ("Length", "length"), ("Lengths", "lengths"))

    def getExecutionCode(self):
        return "length = len(text)"
