import bpy
from ... base_types import AnimationNode

class GetListLengthNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetListLengthNode"
    bl_label = "Get List Length"
    dynamicLabelType = "HIDDEN_ONLY"

    def create(self):
        self.newInput("Generic", "List", "list")
        self.newOutput("Integer", "Length", "length")

    def drawLabel(self):
        return "Get Length"

    def getExecutionCode(self, required):
        yield "try: length = len(list)"
        yield "except: length = 0"
