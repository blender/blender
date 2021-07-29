import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode, AutoSelectListDataType
from ... sockets.info import isBase, toBaseDataType, toListDataType

class GetListElementNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetListElementNode"
    bl_label = "Get List Element"
    bl_width_default = 170
    dynamicLabelType = "HIDDEN_ONLY"

    assignedType = StringProperty(default = "Float", update = AnimationNode.refresh)

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
        baseDataType = self.assignedType
        listDataType = toListDataType(self.assignedType)

        self.newInput(listDataType, "List", "inList")
        self.newInputGroup(self.useIndexList,
            ("Integer", "Index", "index"),
            ("Integer List", "Indices", "indices"))
        self.newInput(baseDataType, "Fallback", "fallback", hide = True)

        self.newOutputGroup(self.useIndexList,
            (baseDataType, "Element", "element"),
            (listDataType, "Elements", "elements"))

        self.newSocketEffect(AutoSelectListDataType("assignedType", "BASE",
            [(self.inputs[0], "LIST"),
             (self.inputs[2], "BASE"),
             (self.outputs[0], "LIST" if self.useIndexList else "BASE")]
        ))

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

    def getExecutionCode(self):
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
