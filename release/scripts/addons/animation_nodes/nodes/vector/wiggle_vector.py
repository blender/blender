import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class VectorWiggleNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorWiggleNode"
    bl_label = "Vector Wiggle"

    nodeSeed = IntProperty(update = propertyChanged)

    createList = BoolProperty(name = "Create List", default = False,
        update = AnimationNode.refresh)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        if self.createList:
            self.newInput("Integer", "Count", "count", value = 5)
        self.newInput("Float", "Evolution", "evolution")
        self.newInput("Float", "Speed", "speed", value = 1, minValue = 0)
        self.newInput("Vector", "Amplitude", "amplitude", value = [5, 5, 5])
        self.newInput("Integer", "Octaves", "octaves", value = 2, minValue = 0)
        self.newInput("Float", "Persistance", "persistance", value = 0.3)

        if self.createList:
            self.newOutput("Vector List", "Vectors", "vectors")
        else:
            self.newOutput("Vector", "Vector", "vector")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "nodeSeed", text = "Node Seed")
        row.prop(self, "createList", text = "", icon = "LINENUMBERS_ON")

    def getExecutionCode(self):
        if self.createList:
            yield "_seed = seed * 23452 + self.nodeSeed * 643523"
            yield "vectors = algorithms.perlin_noise.wiggleVectorList(count, _seed + evolution * speed / 20, amplitude, octaves, persistance)"
        else:
            yield ("vector = Vector(algorithms.perlin_noise.perlinNoiseVectorForNodes("
                   "seed, self.nodeSeed, evolution, speed, amplitude, octaves, persistance))")

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
