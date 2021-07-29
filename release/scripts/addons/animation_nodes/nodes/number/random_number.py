import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from . c_utils import random_DoubleList

class RandomNumberNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomNumberNode"
    bl_label = "Random Number"
    bl_width_default = 150

    nodeSeed = IntProperty(update = propertyChanged)

    createList = BoolProperty(name = "Create List", default = False,
        description = "Create a list of random numbers",
        update = AnimationNode.refresh)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        if self.createList:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Integer", "Count", "count", value = 5, minValue = 0)
            self.newInput("Float", "Min", "minValue", value = 0.0)
            self.newInput("Float", "Max", "maxValue", value = 1.0)
            self.newOutput("Float List", "Numbers", "numbers")
        else:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Float", "Min", "minValue", value = 0.0)
            self.newInput("Float", "Max", "maxValue", value = 1.0)
            self.newOutput("Float", "Number", "number")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "nodeSeed", text = "Node Seed")
        row.prop(self, "createList", text = "", icon = "LINENUMBERS_ON")

    def getExecutionCode(self):
        if self.createList:
            yield "numbers = self.calcRandomNumbers(seed, count, minValue, maxValue)"
        else:
            yield "number = algorithms.random.uniformRandomNumberWithTwoSeeds(seed, self.nodeSeed, minValue, maxValue)"

    def calcRandomNumbers(self, seed, count, minValue, maxValue):
        _seed = seed * 234123 + self.nodeSeed * 1234434
        return random_DoubleList(_seed, count, minValue, maxValue)

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
