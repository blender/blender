import bpy
from ... sockets.info import isList
from ... base_types import AnimationNode, ListTypeSelectorSocket

class ReverseListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReverseListNode"
    bl_label = "Reverse List"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float List")

    def create(self):
        prop = ("assignedType", "LIST")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop, dataIsModified = True))
        self.newOutput(ListTypeSelectorSocket(
            "Reversed List", "reversedList", "LIST", prop))

    def getExecutionCode(self, required):
        yield "reversedList = AN.algorithms.lists.reverse('%s', inList)" % self.assignedType

    def assignType(self, listDataType):
        if not isList(listDataType): return
        if listDataType == self.assignedType: return
        self.assignedType = listDataType
        self.refresh()
