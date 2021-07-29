import bpy
from bpy.props import *
from ... sockets.info import getCopyFunction
from ... algorithms.lists import mask as maskList
from ... base_types import AnimationNode, AutoSelectListDataType

class MaskListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MaskListNode"
    bl_label = "Mask List"

    errorMessage = StringProperty()

    assignedType = StringProperty(default = "Integer List", update = AnimationNode.refresh)

    def create(self):
        self.newInput(self.assignedType, "List", "inList")
        self.newInput("Boolean List", "Mask", "mask")
        self.newOutput(self.assignedType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, inList, mask):
        self.errorMessage = ""
        if len(inList) == len(mask):
            return maskList(self.assignedType, inList, mask)
        else:
            self.errorMessage = "lists have different length"
            return getCopyFunction(self.assignedType)(inList)
