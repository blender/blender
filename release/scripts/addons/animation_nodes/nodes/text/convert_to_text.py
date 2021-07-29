import bpy
from ... base_types import AnimationNode

class ConvertToTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertToTextNode"
    bl_label = "Convert to Text"

    def create(self):
        self.newInput("Generic", "Data", "data")
        self.newOutput("Text", "Text", "text")

    def getExecutionCode(self):
        return "text = str(data)"
