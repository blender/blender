import bpy
from bpy.props import *
from ... sockets.info import isList
from ... events import executionCodeChanged
from ... base_types import AnimationNode, AutoSelectListDataType

repetitionTypeItems = [
    ("LOOP", "Loop", "Repeat", "NONE", 0),
    ("PING_PONG", "Ping Pong", "Repeat and Reverse", "NONE", 1)]

amountTypeItems = [
    ("AMOUNT", "Amount", "Repeat N Times", "NONE", 0),
    ("LENGTH", "Length", "Repeat up to length and cut off the rest", "NONE", 1),
    ("LENGTH_FLOOR", "Below Length", "Repeat up to Length, needs fill till length", "NONE", 2),
    ("LENGTH_CEIL", "Above Length", "Repeat over Length, needs slice down to length", "NONE", 3)]

class RepeatListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RepeatListNode"
    bl_label = "Repeat List"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float List")

    repetitionType = EnumProperty(name = "Repeat Type", default = "LOOP",
        items = repetitionTypeItems, update = executionCodeChanged)

    amountType = EnumProperty(name = "Amount Type", default = "AMOUNT",
        items = amountTypeItems, update = AnimationNode.refresh)

    makeElementCopies = BoolProperty(name = "Make Element Copies", default = True,
        description = "Insert copies of the original elements",
        update = executionCodeChanged)

    def create(self):
        listDataType = self.assignedType

        self.newInput(listDataType, "List", "inList")
        if self.amountType == "AMOUNT":
            self.newInput("Integer", "Amount", "amount", value = 5, minValue = 0)
        elif "LENGTH" in self.amountType:
            self.newInput("Integer", "Length", "length", value = 20, minValue = 0)
        self.newOutput(listDataType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "repetitionType", text = "")
        col.prop(self, "amountType", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "makeElementCopies")
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        yield "inLength = len(inList)"
        yield "if inLength == 0:"
        yield "    outList = self.outputs[0].getDefaultValue()"
        yield "else:"

        if self.amountType == "AMOUNT":
            yield "    outLength = inLength * amount"
        elif self.amountType == "LENGTH":
            yield "    outLength = length"
        elif self.amountType == "LENGTH_FLOOR":
            yield "    outLength = int(math.floor(length / inLength) * inLength)"
        elif self.amountType == "LENGTH_CEIL":
            yield "    outLength = int(math.ceil(length / inLength) * inLength)"
        yield "    outLength = max(0, outLength)"

        if self.repetitionType == "LOOP":
            yield "    _sourceList = inList"
        elif self.repetitionType == "PING_PONG":
            yield "    _reversedList = AN.algorithms.lists.reverse('%s', inList)" % self.assignedType
            yield "    _sourceList = inList + _reversedList"

        yield "    outList = AN.algorithms.lists.repeat('%s', _sourceList, outLength)" % self.assignedType

    def getUsedModules(self):
        return ["math"]

    def assignListDataType(self, listDataType):
        self.assignType(listDataType)

    def assignType(self, listDataType):
        if not isList(listDataType): return
        if listDataType == self.assignedType: return
        self.assignedType = listDataType
