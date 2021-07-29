import bpy
from ... base_types import AnimationNode

class DelayTimeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_DelayTimeNode"
    bl_label = "Delay Time"
    dynamicLabelType = "HIDDEN_ONLY"

    def create(self):
        self.newInput("Float", "Time", "time")
        self.newInput("Float", "Delay", "delay", value = 10)
        self.newOutput("Float", "Time", "outTime")

    def drawLabel(self):
        delaySocket = self.inputs["Delay"]
        if delaySocket.isUnlinked:
            value = delaySocket.value
            if value == int(value): return "Delay " + str(int(value)) + " Frames"
            else: return "Delay " + str(round(value, 2)) + " Frames"
        else: return "Delay Time"

    def getExecutionCode(self):
        return "outTime = time - delay"
