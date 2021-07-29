import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

items = [("ROUND", "Round", ""),
         ("CEILING", "Ceiling", "The smallest integer that is larger than the input (4.3 -> 5)"),
         ("FLOOR", "Floor", "The largest integer that is smaller than the input (5.8 -> 5)")]

class FloatToIntegerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FloatToIntegerNode"
    bl_label = "Float to Integer"
    dynamicLabelType = "ALWAYS"

    type = EnumProperty(name = "Conversion Type", items = items, default = "FLOOR", update = executionCodeChanged)

    def create(self):
        self.newInput("Float", "Float", "float")
        self.newOutput("Integer", "Integer", "integer")

    def drawLabel(self):
        return "to Integer ({})".format(self.type.lower())

    def drawAdvanced(self, layout):
        layout.prop(self, "type", text = "")

    def getExecutionCode(self):
        if self.type == "ROUND": return "integer = int(round(float))"
        if self.type == "CEILING": return "integer = int(math.ceil(float))"
        if self.type == "FLOOR": return "integer = int(math.floor(float))"

    def getUsedModules(self):
        return ["math"]
