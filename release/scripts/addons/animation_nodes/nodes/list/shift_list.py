import bpy
from bpy.props import *
from ... sockets.info import isList
from ... base_types import AnimationNode, AutoSelectListDataType

class ShiftListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ShiftListNode"
    bl_label = "Shift List"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float List")

    def create(self):
        listDataType = self.assignedType

        self.newInput(listDataType, "List", "inList", dataIsModified = True)
        self.newInput("Integer", "Amount", "amount")
        self.newOutput(listDataType, "Shifted List", "shiftedList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def getExecutionCode(self):
        yield "if len(inList) == 0: shiftedList = self.outputs[0].getDefaultValue()"
        yield "else:"
        yield "    shiftAmount = amount % len(inList)"
        yield "    shiftedList = inList[-shiftAmount:] + inList[:-shiftAmount]"
