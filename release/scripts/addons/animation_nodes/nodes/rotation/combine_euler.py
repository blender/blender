import bpy
from bpy.props import *
from . c_utils import combineEulerList
from ... events import executionCodeChanged
from ... data_structures import VirtualDoubleList
from ... base_types import AnimationNode, VectorizedSocket

class CombineEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineEulerNode"
    bl_label = "Combine Euler"

    useListX = VectorizedSocket.newProperty()
    useListY = VectorizedSocket.newProperty()
    useListZ = VectorizedSocket.newProperty()

    useDegree = BoolProperty(name = "Use Degree", default = False,
        update = executionCodeChanged)

    def create(self):
        self.newInput(VectorizedSocket("Float", "useListX", ("X", "x"), ("X", "x")))
        self.newInput(VectorizedSocket("Float", "useListY", ("Y", "y"), ("Y", "y")))
        self.newInput(VectorizedSocket("Float", "useListZ", ("Z", "z"), ("Z", "z")))

        self.newOutput(VectorizedSocket("Euler",
            ["useListX", "useListY", "useListZ"],
            ("Euler", "euler"), ("Eulers", "eulers")))

    def draw(self, layout):
        layout.prop(self, "useDegree")

    def getExecutionCode(self, required):
        if self.generatesList:
            yield "eulers = self.createEulerList(x, y, z)"
        else:
            yield self.getExecutionCode_Single()

    def getExecutionCode_Single(self):
        if self.useDegree:
            toRadian = "math.pi / 180"
            return "euler = Euler((x * {0}, y * {0}, z * {0}), 'XYZ')".format(toRadian)
        else:
            return "euler = Euler((x, y, z), 'XYZ')"

    def createEulerList(self, x, y, z):
        x, y, z = VirtualDoubleList.createMultiple((x, 0), (y, 0), (z, 0))
        amount = VirtualDoubleList.getMaxRealLength(x, y, z)
        return combineEulerList(amount, x, y, z, self.useDegree)

    def getUsedModules(self):
        return ["math"]

    @property
    def generatesList(self):
        return any((self.useListX, self.useListY, self.useListZ))