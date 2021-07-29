import bpy
from bpy.props import *
from ... ui.info_popups import showTextPopup
from ... events import executionCodeChanged
from ... base_types import AnimationNode, AutoSelectListDataType
from ... sockets.info import toBaseDataType, isBase, isComparable, toListDataType, isList

removeTypeItems = [
    ("FIRST_OCCURRENCE", "First Occurrence", "", "", 0),
    ("ALL_OCCURRENCES", "All Occurrences", "", "", 1),
    ("INDEX", "Index", "", "", 2) ]

class RemoveListElementNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RemoveListElementNode"
    bl_label = "Remove List Element"

    def typeChanged(self, context):
        if self.isAllowedDataType(self.assignedType):
            self.refresh()
        else:
            self.reset_and_show_error()

    assignedType = StringProperty(update = typeChanged, default = "Integer")

    removeType = EnumProperty(name = "Remove Type", default = "FIRST_OCCURRENCE",
        items = removeTypeItems, update = typeChanged)

    def create(self):
        baseDataType = self.assignedType
        listDataType = toListDataType(self.assignedType)

        if self.removeType in ("FIRST_OCCURRENCE", "INDEX"):
            self.newInput(listDataType, "List", "inList", dataIsModified = True)
        else:
            self.newInput(listDataType, "List", "inList")

        if self.removeType in ("FIRST_OCCURRENCE", "ALL_OCCURRENCES"):
            self.newInput(baseDataType, "Element", "element", defaultDrawType = "PREFER_PROPERTY")
        elif self.removeType == "INDEX":
            self.newInput("Integer", "Index", "index")

        self.newOutput(listDataType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "BASE",
            [(self.inputs[0], "LIST"),
             (self.inputs[1], "BASE") if self.inputs[1].name == "Element" else (None, "IGNORE"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "removeType", text = "")

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignListDataType",
            dataTypes = "LIST", text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        yield "outList = inList"
        if self.removeType == "FIRST_OCCURRENCE":
            yield "try: inList.remove(element)"
            yield "except ValueError: pass"
        elif self.removeType == "ALL_OCCURRENCES":
            yield "outList = [e for e in inList if e != element]"
        elif self.removeType == "INDEX":
            yield "if 0 <= index < len(inList): del inList[index]"

    def assignListDataType(self, listDataType):
        self.assignType(toBaseDataType(listDataType))

    def assignType(self, baseDataType):
        if not isBase(baseDataType): return
        if baseDataType == self.assignedType: return
        self.assignedType = baseDataType

    def isAllowedDataType(self, dataType):
        if self.removeType in ("FIRST_OCCURRENCE", "ALL_OCCURRENCES"):
            if not isComparable(dataType):
                return False
        return True

    def reset_and_show_error(self):
        self.show_type_error(self.assignedType)
        self.removeLinks()
        self.assignedType = "Integer"

    def show_type_error(self, dataType):
        text = "This list type only supports element removal using an index: '{}'".format(toListDataType(dataType))
        showTextPopup(text = text, title = "Error", icon = "ERROR")
