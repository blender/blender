import bpy
from ... base_types import AnimationNode

class JoinTextsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_JoinTextsNode"
    bl_label = "Join Texts"

    def create(self):
        self.newInput("Text List", "Texts", "texts")
        self.newInput("Text", "Separator", "separator")
        self.newOutput("Text", "Text", "text")

    def getExecutionCode(self):
        return "text = separator.join(texts)"
