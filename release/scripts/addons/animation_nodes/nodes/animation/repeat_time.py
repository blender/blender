import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

repetitionTypeItems = [
    ("LOOP", "Loop", ""),
    ("PING_PONG", "Ping Pong", "")]

class RepeatTimeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RepeatTimeNode"
    bl_label = "Repeat Time"

    repetitionType = EnumProperty(name = "Repetition Type", default = "LOOP",
        items = repetitionTypeItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Float", "Time", "time")
        self.newInput("Float", "Rate", "rate", value = 50, minValue = 0.0001)
        self.newOutput("Float", "Time", "outTime")

    def draw(self, layout):
        layout.prop(self, "repetitionType", text = "Type")

    def getExecutionCode(self):
        if self.repetitionType == "LOOP":
            yield "outTime = time % max(rate, 0.0001)"
        if self.repetitionType == "PING_PONG":
            yield "finalRate = max(rate, 0.0001)"
            yield "outTime = time % finalRate if (time // finalRate) % 2 == 0 else finalRate - time % finalRate"
        yield "outTime = time if time < 0 else outTime"
