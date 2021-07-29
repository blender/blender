import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode, AutoSelectListDataType
from ... sockets.info import isBase, toBaseDataType, toListDataType

fillModeItems = [
    ("LEFT", "Left", "", "TRIA_LEFT", 0),
    ("RIGHT", "Right", "", "TRIA_RIGHT", 1) ]

class FillListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FillListNode"
    bl_label = "Fill List"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float")

    fillMode = EnumProperty(name = "Fill Mode", default = "RIGHT",
        items = fillModeItems, update = executionCodeChanged)

    makeElementCopies = BoolProperty(name = "Make Element Copies", default = True,
        description = "Insert copies of the original fill element",
        update = executionCodeChanged)

    def create(self):
        baseDataType = self.assignedType
        listDataType = toListDataType(self.assignedType)

        self.newInput(listDataType, "List", "inList", dataIsModified = True)
        self.newInput(baseDataType, "Element", "fillElement")
        self.newInput("Integer", "Length", "length", minValue = 0)
        self.newOutput(listDataType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "BASE",
            [(self.inputs[0], "LIST"),
             (self.inputs[1], "BASE"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "fillMode", expand = True)

    def drawAdvanced(self, layout):
        col = layout.column()
        col.active = self.inputs["Element"].isCopyable()
        col.prop(self, "makeElementCopies")
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        yield ("outList = AN.algorithms.lists.fill('{}', inList, '{}', length, fillElement, {})"
               .format(toListDataType(self.assignedType), self.fillMode, self.makeElementCopies))

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
