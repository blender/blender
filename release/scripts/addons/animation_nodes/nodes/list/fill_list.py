import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode, ListTypeSelectorSocket
from ... sockets.info import isBase, toBaseDataType, toListDataType

fillModeItems = [
    ("LEFT", "Left", "", "TRIA_LEFT", 0),
    ("RIGHT", "Right", "", "TRIA_RIGHT", 1) ]

class FillListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FillListNode"
    bl_label = "Fill List"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float")

    fillMode = EnumProperty(name = "Fill Mode", default = "RIGHT",
        items = fillModeItems, update = executionCodeChanged)

    makeElementCopies = BoolProperty(name = "Make Element Copies", default = True,
        description = "Insert copies of the original fill element",
        update = executionCodeChanged)

    def create(self):
        prop = ("assignedType", "BASE")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop, dataIsModified = True))
        self.newInput(ListTypeSelectorSocket(
            "Element", "fillElement", "BASE", prop))
        self.newInput("Integer", "Length", "length", minValue = 0)

        self.newOutput(ListTypeSelectorSocket(
            "List", "outList", "LIST", prop))

    def draw(self, layout):
        layout.prop(self, "fillMode", expand = True)

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.inputs["Element"].isCopyable()
        col.prop(self, "makeElementCopies")
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self, required):
        yield ("outList = AN.algorithms.lists.fill('{}', inList, '{}', length, fillElement, {})"
               .format(toListDataType(self.assignedType), self.fillMode, self.makeElementCopies))

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
        self.refresh()
