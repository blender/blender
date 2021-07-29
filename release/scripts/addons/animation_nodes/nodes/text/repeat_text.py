import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

modeItems = [
    ("START", "at Start", "", "TRIA_LEFT", 0),
    ("END", "at End", "", "TRIA_RIGHT", 1)
]

class RepeatTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RepeatTextNode"
    bl_label = "Repeat Text"

    mode = EnumProperty(name = "Mode", default = "END",
        items = modeItems, update = propertyChanged)

    def create(self):
        self.newInput("Integer", "Repeats", "repeats", minValue = 0)
        self.newInput("Text", "Text", "inText")
        self.newInput("Text", "Fill", "fill")

        self.newOutput("Text", "Text", "outText")

    def draw(self, layout):
        layout.prop(self, "mode", text = "")

    def execute(self, repeats, inText, fill):
        if self.mode == "START":
            return (fill * repeats) + inText
        elif self.mode == "END":
            return inText + (fill * repeats)
