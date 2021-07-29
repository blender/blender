import bpy
from ... base_types import AnimationNode

class RoundNumberNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RoundNumberNode"
    bl_label = "Round Number"

    def create(self):
        self.newInput("Float", "Number", "number")
        self.newInput("Integer", "Decimals", "decimals")
        self.newOutput("Float", "Result", "result")

    def getExecutionCode(self):
        yield "result  = int(round(number, decimals)) if decimals <= 0 else round(number, decimals)"
