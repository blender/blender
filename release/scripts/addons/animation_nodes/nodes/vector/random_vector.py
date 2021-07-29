import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... algorithms.lists.random import generateRandomVectors

class RandomVectorNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomVectorNode"
    bl_label = "Random Vector"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged, max = 1000, min = 0)

    createList = BoolProperty(name = "Create List", default = False,
        description = "Create a list of random vectors",
        update = AnimationNode.refresh)

    normalizedVector = BoolProperty(name = "Normalized Vector", default = False,
        update = AnimationNode.refresh)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        if self.createList:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Integer", "Count", "count", value = 5, minValue = 0)
            self.newInput("Float", "Scale", "scale", value = 2.0)
            self.newOutput("Vector List", "Vectors", "randomVectors")
        else:
            self.newInput("Integer", "Seed", "seed")
            self.newInput("Float", "Scale", "scale", value = 2.0)
            self.newOutput("Vector", "Vector", "randomVector")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "nodeSeed", text = "Node Seed")
        row.prop(self, "createList", text = "", icon = "LINENUMBERS_ON")

    def drawAdvanced(self, layout):
        layout.prop(self, "normalizedVector")

    def getExecutionCode(self):
        if self.createList:
            yield "randomVectors = self.calcRandomVectors(seed, count, scale)"
        else:
            yield "_seed = seed + 25642 * self.nodeSeed"
            if self.normalizedVector:
                yield "randomVector = algorithms.random.getRandomNormalized3DVector(_seed, scale)"
            else:
                yield "randomVector = algorithms.random.getRandom3DVector(_seed, scale)"

    def calcRandomVectors(self, seed, count, scale):
        return generateRandomVectors(seed + self.nodeSeed * 234144, count, scale, self.normalizedVector)

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
