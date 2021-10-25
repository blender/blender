import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... sockets.info import isBase, toBaseDataType
from ... base_types import AnimationNode, ListTypeSelectorSocket

class GetListElementNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetListElementNode"
    bl_label = "Get List Element"
    bl_width_default = 180
    dynamicLabelType = "HIDDEN_ONLY"

    assignedType = ListTypeSelectorSocket.newProperty(default = "Float")

    clampIndex = BoolProperty(name = "Clamp Index", default = False,
        description = "Clamp the index between the lowest and highest possible index",
        update = executionCodeChanged)

    allowNegativeIndex = BoolProperty(name = "Allow Negative Index",
        description = "-2 means the second last list element",
        update = executionCodeChanged, default = True)

    makeCopy = BoolProperty(name = "Make Copy", default = True,
        description = "Output a copy of the list element to make it independed",
        update = executionCodeChanged)

    useIndexList = BoolProperty(name = "Use Index List", default = False,
        update = AnimationNode.refresh)

    def create(self):
        prop = ("assignedType", "BASE")
        self.newInput(ListTypeSelectorSocket(
            "List", "inList", "LIST", prop))

        if not self.useIndexList:
            self.newInput("Integer", "Index", "index")
        else:
            self.newInput("Integer List", "Indices", "indices")

        self.newInput(ListTypeSelectorSocket(
            "Fallback", "fallback", "BASE", prop, hide = True))

        if not self.useIndexList:
            self.newOutput(ListTypeSelectorSocket(
                "Element", "element", "BASE", prop))
        else:
            self.newOutput(ListTypeSelectorSocket(
                "Elements", "elements", "LIST", prop))

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "clampIndex", text = "Clamp", icon = "FULLSCREEN_EXIT")
        row.prop(self, "allowNegativeIndex", text = "Wrap", icon = "LOOP_FORWARDS")
        row.prop(self, "useIndexList", text = "", icon = "LINENUMBERS_ON")

    def drawAdvanced(self, layout):
        layout.prop(self, "makeCopy")
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def drawLabel(self):
        if not self.useIndexList:
            if self.inputs["Index"].isUnlinked:
                return "List[{}]".format(self.inputs["Index"].value)
        return "Get List Element"

    def getExecutionCode(self, required):
        if self.useIndexList:
            yield from self.getExecutionCode_List()
        else:
            yield from self.getExecutionCode_Single()

    def getExecutionCode_Single(self):
        yield "if len(inList) != 0: element = " + self.getGetElementCode("index", "len(inList)")
        yield "else: element = fallback"

        if self.makeCopy:
            socket = self.outputs[0]
            if socket.isCopyable():
                yield "element = " + socket.getCopyExpression().replace("value", "element")

    def getExecutionCode_List(self):
        yield "if len(inList) != 0:"
        yield "    length = len(inList)"
        yield "    elements = self.outputs[0].getDefaultValue()"
        yield "    for i in indices:"
        yield "        elements.append({})".format(self.getGetElementCode("i", "length"))
        yield "else:"
        fromFallbackCode = self.sockets[0].getFromValuesCode().replace("value", "[fallback]")
        yield "    elements = {} * len(indices)".format(fromFallbackCode)

    def getGetElementCode(self, index, length):
        if self.allowNegativeIndex:
            if self.clampIndex:
                code = "inList[min(max({index}, -{length}), {length} - 1)]"
            else:
                code = "inList[{index}] if -{length} <= {index} < {length} else fallback"
        else:
            if self.clampIndex:
                code = "inList[min(max({index}, 0), {length} - 1)]"
            else:
                code = "inList[{index}] if 0 <= {index} < {length} else fallback"
        return code.format(index = index, length = length)

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType
        self.refresh()
