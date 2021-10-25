import bpy
from ... base_types import AnimationNode, ListTypeSelectorSocket

class ShiftListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ShiftListNode"
    bl_label = "Shift List"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float List")

    def create(self):
        prop = ("assignedType", "LIST")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop, dataIsModified = True))
        self.newInput("Integer", "Amount", "amount")
        self.newOutput(ListTypeSelectorSocket(
            "Shifted List", "shiftedList", "LIST", prop, dataIsModified = True))

    def getExecutionCode(self, required):
        yield "if len(inList) == 0: shiftedList = self.outputs[0].getDefaultValue()"
        yield "else:"
        yield "    shiftAmount = amount % len(inList)"
        yield "    shiftedList = inList[-shiftAmount:] + inList[:-shiftAmount]"
