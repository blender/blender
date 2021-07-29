import bpy
from bpy.props import *
from ... sockets.info import isList
from ... base_types import AnimationNode, AutoSelectListDataType

class ReverseListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReverseListNode"
    bl_label = "Reverse List"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float List")

    def create(self):
        listDataType = self.assignedType
        self.newInput(listDataType, "List", "inList", dataIsModified = True)
        self.newOutput(listDataType, "Reversed List", "reversedList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def getExecutionCode(self):
        yield "reversedList = AN.algorithms.lists.reverse('%s', inList)" % self.assignedType

    def assignType(self, listDataType):
        if not isList(listDataType): return
        if listDataType == self.assignedType: return
        self.assignedType = listDataType
