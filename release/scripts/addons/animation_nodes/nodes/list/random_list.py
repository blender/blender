import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode, AutoSelectListDataType
from ... algorithms.lists import getRepeatFunction, getShuffleFunction

class RandomListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RandomListNode"
    bl_label = "Random List"

    assignedType = StringProperty(default = "Float List",
        update = AnimationNode.refresh)

    nodeSeed = IntProperty(name = "Node Seed", update = propertyChanged)

    def create(self):
        self.newInput("Integer", "Seed", "seed")
        self.newInput("Integer", "Length", "length", value = 5, minValue = 0)
        self.newInput(self.assignedType, "Source", "sourceElements", dataIsModified = True)
        self.newOutput(self.assignedType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[2], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "nodeSeed")

    def execute(self, seed, length, sourceElements):
        if len(sourceElements) == 0 or length < 0:
            return self.outputs[0].getDefaultValue()

        repeat = getRepeatFunction(self.assignedType)
        shuffle = getShuffleFunction(self.assignedType)

        seed = self.nodeSeed * 987654 + seed * 12345
        elements = shuffle(sourceElements, seed)
        return shuffle(repeat(elements, length), seed + 1234)
