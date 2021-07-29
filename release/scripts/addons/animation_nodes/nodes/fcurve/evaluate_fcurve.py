import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged

frameTypeItems = [
    ("OFFSET", "Offset", ""),
    ("ABSOLUTE", "Absolute", "") ]

class EvaluateFCurveNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EvaluateFCurveNode"
    bl_label = "Evaluate FCurve"

    frameType = EnumProperty(
        name = "Frame Type", default = "OFFSET",
        items = frameTypeItems, update = executionCodeChanged)

    def create(self):
        self.newInput("FCurve", "FCurve", "fCurve")
        self.newInput("Float", "Frame", "frame")
        self.newOutput("Float", "Value", "value")

    def draw(self, layout):
        layout.prop(self, "frameType", text = "Frame")

    def getExecutionCode(self):
        yield "evaluationFrame = frame"
        if self.frameType == "OFFSET":
            yield "evaluationFrame += self.nodeTree.scene.frame_current_final"

        yield "if fCurve is None: value = 0.0"
        yield "else: value = fCurve.evaluate(evaluationFrame)"
