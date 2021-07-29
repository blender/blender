import bpy
import random
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, AutoSelectListDataType

class ShuffleListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ShuffleListNode"
    bl_label = "Shuffle List"

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged)

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float List")

    def setup(self):
        self.randomizeNodeSeed()

    def create(self):
        listDataType = self.assignedType

        self.newInput(listDataType, "List", "sourceList", dataIsModified = True)
        self.newInput("an_IntegerSocket", "Seed", "seed")
        self.newOutput(listDataType, "Shuffled List", "newList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def getExecutionCode(self):
        yield "_seed = self.nodeSeed * 3242354 + seed"
        yield "newList = AN.algorithms.lists.shuffle('%s', sourceList, _seed)" % self.assignedType

    def duplicate(self, sourceNode):
        self.randomizeNodeSeed()

    def randomizeNodeSeed(self):
        self.nodeSeed = int(random.random() * 100)
