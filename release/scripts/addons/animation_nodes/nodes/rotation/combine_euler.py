import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

class CombineEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineEulerNode"
    bl_label = "Combine Euler"

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    def create(self):
        self.newInput("Float", "X", "x")
        self.newInput("Float", "Y", "y")
        self.newInput("Float", "Z", "z")
        self.newOutput("Euler", "Euler", "euler")

    def draw(self, layout):
        layout.prop(self, "useDegree")

    def getExecutionCode(self):
        if self.useDegree:
            toRadian = "math.pi / 180"
            return "euler = Euler((x * {0}, y * {0}, z * {0}), 'XYZ')".format(toRadian)
        else:
            return "euler = Euler((x, y, z), 'XYZ')"

    def getUsedModules(self):
        return ["math"]