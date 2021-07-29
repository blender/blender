import bpy
import math
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged

constantItems = [
    ("Pi", "Pi", str(math.pi)[:10] + "...", "NONE", 0),
    ("e", "e", str(math.e)[:10] + "...", "NONE", 1)
]

factors = ["1/4", "1/3", "1/2", "1", "2", "3", "4"]
factorItems = [(c, c, "", "NONE", i) for i, c in enumerate(factors)]

class NumberConstantsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_NumberConstantsNode"
    bl_label = "Constants"
    dynamicLabelType = "HIDDEN_ONLY"

    constant = EnumProperty(name = "Constant", default = "Pi",
        items = constantItems, update = executionCodeChanged)

    factor = EnumProperty(name = "Factor", default = "1",
        items = factorItems, update = executionCodeChanged)

    def create(self):
        self.newOutput("Float", "Value", "value")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "factor", text = "")
        row.prop(self, "constant", text = "")

    def drawLabel(self):
        if self.factor == "1":
            return self.constant
        return self.factor + " " + self.constant

    def getExecutionCode(self):
        if self.constant == "Pi":
            yield "value = math.pi"
        elif self.constant == "e":
            yield "value = math.e"

        if self.factor != "1":
            yield "value *= " + self.factor

    def getUsedModules(self):
        return ["math"]
