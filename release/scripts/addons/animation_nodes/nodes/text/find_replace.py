import bpy
from ... base_types import AnimationNode

class ReplaceTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReplaceTextNode"
    bl_label = "Replace Text"

    def create(self):
        self.newInput("Text", "Text", "text")
        self.newInput("Text", "Old", "old")
        self.newInput("Text", "New", "new")
        self.newOutput("Text", "Text", "newText")

    def getExecutionCode(self):
        return "newText = text.replace(old, new)"
