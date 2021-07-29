import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... sockets.info import isBase, toBaseDataType, toListDataType
from ... base_types import AnimationNode, AutoSelectListDataType

class SearchListElementNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SearchListElementNode"
    bl_label = "Search List Element"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float")

    def create(self):
        baseDataType = self.assignedType
        listDataType = toListDataType(self.assignedType)

        self.newInput(listDataType, "List", "list", dataIsModified  = True)
        self.newInput(baseDataType, "Search", "search", dataIsModified = True)

        self.newOutput("an_IntegerSocket", "First Index", "firstIndex")
        self.newOutput("an_IntegerListSocket", "All Indices", "allIndices")
        self.newOutput("an_IntegerSocket", "Occurrences", "occurrences")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "BASE",
            [(self.inputs[0], "LIST"),
             (self.inputs[1], "BASE")]
        ))

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        if isLinked["allIndices"]:
            yield "allIndices = LongList.fromValues(i for i, element in enumerate(list) if element == search)"
            if isLinked["firstIndex"]:  yield "firstIndex = allIndices[0] if len(allIndices) > 0 else -1"
            if isLinked["occurrences"]: yield "occurrences = len(allIndices)"
        else:
            if isLinked["firstIndex"]:
                yield "try: firstIndex = list.index(search)"
                yield "except: firstIndex = -1"
            if isLinked["occurrences"]:
                yield "occurrences = list.count(search)"

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
