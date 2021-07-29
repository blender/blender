import bpy
import random
from bpy.props import *
from math import radians
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... algorithms.lists.random import generateRandomEulers

class RandomEulerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomEulerNode"
    bl_label = "Random Euler"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged, max = 1000, min = 0)

    createList = BoolProperty(name = "Create List", default = False,
        description = "Create a list of random eulers",
        update = AnimationNode.refresh)

    def create(self):
        if self.createList:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Integer", "Count", "count", value = 5, minValue = 0)
            self.newInput("Float", "Scale", "scale", value = 0.5)
            self.newOutput("Euler List", "Eulers", "randomEulers")
        else:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Float", "Scale", "scale", value = 0.5)
            self.newOutput("Euler", "Euler", "randomEuler")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "nodeSeed", text = "Node Seed")
        row.prop(self, "createList", text = "", icon = "LINENUMBERS_ON")

    def getExecutionCode(self):
        yield "randomEuler = Euler(algorithms.random.randomNumberTuple(seed + 45234 * self.nodeSeed, 3, scale))"

    def getExecutionCode(self):
        if self.createList:
            yield "randomEulers = self.calcRandomEulers(seed, count, scale)"
        else:
            yield "randomEuler = Euler(algorithms.random.randomNumberTuple(seed + 45234 * self.nodeSeed, 3, scale))"

    def calcRandomEulers(self, seed, count, scale):
        return generateRandomEulers(self.nodeSeed * 1234545 + seed, count, scale)

    def duplicate(self, sourceNode):
        self.nodeSeed = int(random.random() * 100)
