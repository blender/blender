import bpy
from ... sockets.info import isBase, toBaseDataType
from ... base_types import AnimationNode, ListTypeSelectorSocket

class SearchListElementNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SearchListElementNode"
    bl_label = "Search List Element"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float")

    def create(self):
        prop = ("assignedType", "BASE")

        self.newInput(ListTypeSelectorSocket(
            "List", "list", "LIST", prop, dataIsModified = True))
        self.newInput(ListTypeSelectorSocket(
            "Search", "search", "BASE", prop))

        self.newOutput("Integer", "First Index", "firstIndex")
        self.newOutput("Integer List", "All Indices", "allIndices")
        self.newOutput("Integer", "Occurrences", "occurrences")

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        if "allIndices" in required:
            yield "allIndices = LongList.fromValues(i for i, element in enumerate(list) if element == search)"
            if "firstIndex" in required:  yield "firstIndex = allIndices[0] if len(allIndices) > 0 else -1"
            if "occurrences" in required: yield "occurrences = len(allIndices)"
        else:
            if "firstIndex" in required:
                yield "try: firstIndex = list.index(search)"
                yield "except: firstIndex = -1"
            if "occurrences" in required:
                yield "occurrences = list.count(search)"

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
        self.refresh()
