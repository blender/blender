import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

class RandomTextNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomTextNode"
    bl_label = "Random Text"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged)

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newInput("Integer", "Length", "length", value = 5)
        self.newInput("Text", "Characters", "characters", value = "abcdefghijklmnopqrstuvwxyz")
        self.newOutput("Text", "Text", "text")

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def execute(self, seed, length, characters):
        random.seed(seed + 12334 * self.nodeSeed)
        return ''.join(random.choice(characters) for _ in range(length))

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
