import bpy
from ... algorithms.lists import mask as maskList
from ... base_types import AnimationNode, ListTypeSelectorSocket

class MaskListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MaskListNode"
    bl_label = "Mask List"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Integer List")

    def create(self):
        prop = ("assignedType", "LIST")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop))
        self.newInput("Boolean List", "Mask", "mask")
        self.newOutput(ListTypeSelectorSocket(
            "List", "outList", "LIST", prop))

    def execute(self, inList, mask):
        if len(inList) == len(mask):
            return maskList(self.assignedType, inList, mask)
        else:
            _mask = mask.repeated(length = len(inList), default = True)
            return maskList(self.assignedType, inList, _mask)
