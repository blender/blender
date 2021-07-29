import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class RandomQuaternionNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomQuaternionNode"
    bl_label = "Random Quaternion"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged, max = 1000, min = 0)

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newOutput("Quaternion", "Quaternion", "randomQuaternion")

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def getExecutionCode(self):
        yield "randomQuaternion = Quaternion(algorithms.random.randomNumberTuple(seed + 24523 * self.nodeSeed, 4, math.pi))"
        yield "randomQuaternion.normalize()"

    def getUsedModules(self):
        return ["math"]

    def duplicate(self, sourceNode):
        self.nodeSeed = int(random.random() * 100)
