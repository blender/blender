import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, ListTypeSelectorSocket
from ... algorithms.lists import getRepeatFunction, getShuffleFunction

class RandomListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomListNode"
    bl_label = "Random List"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float List")

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged)

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newInput("Integer", "Length", "length", value = 5, minValue = 0)

        prop = ("assignedType", "LIST")
        self.newInput(ListTypeSelectorSocket(
            "Source", "sourceElements", "LIST", prop, dataIsModified = True))
        self.newOutput(ListTypeSelectorSocket(
            "List", "outList", "LIST", prop))

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def execute(self, seed, length, sourceElements):
        if len(sourceElements) == 0 or length < 0:
            return self.outputs[0].getDefaultValue()

        repeat = getRepeatFunction(self.assignedType)
        shuffle = getShuffleFunction(self.assignedType)

        seed = self.nodeSeed * 987654 + seed * 12345 + length * 5243
        elements = shuffle(sourceElements, seed)
        return shuffle(repeat(elements, length), seed + 1234)
