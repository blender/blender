import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged

class TrimTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TrimTextNode"
    bl_label = "Trim Text"

    trimStart = BoolProperty(name = "Trim Start", default = False,
        update = AnimationNode.refresh)

    trimEnd = BoolProperty(name = "Trim End", default = True,
        update = AnimationNode.refresh)

    allowNegativeIndex = BoolProperty(name = "Allow Negative Index", default = False,
        update = executionCodeChanged)

    def create(self):
        self.newInput("Text", "Text", "text")
        if self.trimStart:
            self.newInput("Integer", "Start", "start", value = 0)
        if self.trimEnd:
            self.newInput("Integer", "End", "end", value = 5)
        self.newOutput("Text", "Text", "outText")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "trimStart", text = "Start", icon = "TRIA_RIGHT")
        row.prop(self, "trimEnd", text = "End", icon = "TRIA_LEFT")

    def drawAdvanced(self, layout):
        layout.prop(self, "allowNegativeIndex", text = "Negative Indices")

    def getExecutionCode(self):
        if not self.trimStart:
            yield "start = 0"
        if not self.trimEnd:
            yield "end = len(text)"

        if self.allowNegativeIndex:
            yield "outText = text[start:end]"
        else:
            yield "outText = text[max(start, 0):max(end, 0)]"
