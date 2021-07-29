import bpy
from bpy.props import *
from ... sockets.info import isBase, toBaseDataType, toListDataType
from ... base_types import AnimationNode, AutoSelectListDataType

class AppendListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_AppendListNode"
    bl_label = "Append to List"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float")

    def create(self):
        baseDataType = self.assignedType
        listDataType = toListDataType(self.assignedType)

        self.newInput(listDataType, "List", "list", dataIsModified  = True)
        self.newInput(baseDataType, "Element", "element", dataIsModified = True)
        self.newOutput(listDataType, "List", "list")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "BASE",
            [(self.inputs[0], "LIST"),
             (self.inputs[1], "BASE"),
             (self.outputs[0], "LIST")]
        ))

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        return "list.append(element)"

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
