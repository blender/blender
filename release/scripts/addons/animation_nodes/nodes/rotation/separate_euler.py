import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

class SeparateEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparateEulerNode"
    bl_label = "Separate Euler"

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    def create(self):
        self.newInput("Euler", "Euler", "euler")
        self.newOutput("Float", "X", "x")
        self.newOutput("Float", "Y", "y")
        self.newOutput("Float", "Z", "z")

    def draw(self, layout):
        layout.prop(self, "useDegree")

    def getExecutionCode(self):
        if self.useDegree:
            toDegree = "180 / math.pi"
            return "x, y, z = euler.x * {0}, euler.y * {0}, euler.z * {0}".format(toDegree)
        else:
            return "x, y, z = euler"

    def getUsedModules(self):
        return ["math"]
