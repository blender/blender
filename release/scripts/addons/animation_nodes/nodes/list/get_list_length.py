import bpy
from ... base_types import AnimationNode

class GetListLengthNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetListLengthNode"
    bl_label = "Get List Length"
    dynamicLabelType = "HIDDEN_ONLY"

    def create(self):
        self.newInput("an_GenericSocket", "List", "list")
        self.newOutput("an_IntegerSocket", "Length", "length")

    def drawLabel(self):
        return "Get Length"

    def getExecutionCode(self):
        return ("try: length = len(list)",
                "except: length = 0")
