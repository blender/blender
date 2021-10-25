import bpy
from bpy.props import *
from ... events import executionCodeChanged
from . c_utils import getAxisListOfEulerList
from ... base_types import AnimationNode, VectorizedSocket

class SeparateEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparateEulerNode"
    bl_label = "Separate Euler"

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    useList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Euler", "useList",
            ("Euler", "euler"), ("Eulers", "eulers")))

        self.newOutput(VectorizedSocket("Float", "useList",
            ("X", "x"), ("X", "x")))
        self.newOutput(VectorizedSocket("Float", "useList",
            ("Y", "y"), ("Y", "y")))
        self.newOutput(VectorizedSocket("Float", "useList",
            ("Z", "z"), ("Z", "z")))

    def draw(self, layout):
        layout.prop(self, "useDegree")

    def getExecutionCode(self, required):
        for i, axis in enumerate("xyz"):
            if axis in required:
                if self.useList:
                    yield "{0} = self.getAxisList(eulers, '{0}')".format(axis)
                else:
                    yield "{} = euler[{}]".format(axis, i)
                    if self.useDegree:
                        yield "{} *= 180 / math.pi".format(axis)

    def getAxisList(self, eulers, axis):
        return getAxisListOfEulerList(eulers, axis, self.useDegree)

    def getUsedModules(self):
        return ["math"]
