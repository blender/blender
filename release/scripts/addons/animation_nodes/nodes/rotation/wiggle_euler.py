import bpy
import random
from bpy.props import *
from math import radians
from mathutils import Euler
from ... events import propertyChanged
from ... base_types import AnimationNode

class EulerWiggleNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EulerWiggleNode"
    bl_label = "Euler Wiggle"

    nodeSeed = IntProperty(update = propertyChanged)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        self.newInput("Float", "Seed", "seed")
        self.newInput("Float", "Evolution", "evolution")
        self.newInput("Float", "Speed", "speed", value = 1, minValue = 0)
        self.newInput("Euler", "Amplitude", "amplitude", value = [radians(30), radians(30), radians(30)])
        self.newInput("Integer", "Octaves", "octaves", value = 2, minValue = 0)
        self.newInput("Float", "Persistance", "persistance", value = 0.3)
        self.newOutput("Euler", "Euler", "euler")

    def draw(self, layout):
        layout.prop(self, "nodeSeed", text = "Node Seed")

    def getExecutionCode(self):
        yield "euler = Euler(algorithms.perlin_noise.perlinNoiseVectorForNodes(seed, self.nodeSeed, evolution, speed, amplitude, octaves, persistance))"

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
