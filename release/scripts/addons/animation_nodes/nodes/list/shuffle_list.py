import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, ListTypeSelectorSocket

class ShuffleListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ShuffleListNode"
    bl_label = "Shuffle List"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged)

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float List")

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        prop = ("assignedType", "LIST")

        self.newInput(ListTypeSelectorSocket(
            "List", "sourceList", "LIST", prop, dataIsModified = True))
        self.newInput("Integer", "Seed", "seed")
        self.newOutput(ListTypeSelectorSocket(
            "Shuffled List", "newList", "LIST", prop))

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def getExecutionCode(self, required):
        yield "_seed = self.nodeSeed * 3242354 + seed"
        yield "newList = AN.algorithms.lists.shuffle('%s', sourceList, _seed)" % self.assignedType

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
