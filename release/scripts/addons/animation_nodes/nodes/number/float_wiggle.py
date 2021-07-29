import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class FloatWiggleNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FloatWiggleNode"
    bl_label = "Number Wiggle"

    nodeSeed = IntProperty(update = propertyChanged)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        self.newInput("Float", "Seed", "seed")
        self.newInput("Float", "Evolution", "evolution")
        self.newInput("Float", "Speed", "speed", value = 1, minValue = 0)
        self.newInput("Float", "Amplitude", "amplitude", value = 1.0)
        self.newInput("Integer", "Octaves", "octaves", value = 2, minValue = 0)
        self.newInput("Float", "Persistance", "persistance", value = 0.3)
        self.newOutput("Float", "Number", "number")

    def draw(self, layout):
        layout.prop(self, "nodeSeed", text = "Node Seed")

    def getExecutionCode(self):
        yield "number = algorithms.perlin_noise.perlinNoiseForNodes(seed, self.nodeSeed, evolution, speed, amplitude, octaves, persistance)"

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
