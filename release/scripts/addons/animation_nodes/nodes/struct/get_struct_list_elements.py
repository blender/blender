import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... sockets.info import isBase, toListDataType
from ... events import executionCodeChanged, propertyChanged

class GetStructListElementsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_GetStructListElementsNode"
    bl_label = "Get Struct List Elements"
    bl_width_default = 160
    errorHandlingType = "MESSAGE"

    makeCopies = BoolProperty(name = "Make Copies", default = True,
        description = "Copy the data before outputting it",
        update = executionCodeChanged)

    elementKey = StringProperty(name = "Key", update = propertyChanged)

    elementDataType = StringProperty(name = "Data Type", default = "Integer",
        update = AnimationNode.refresh)

    def create(self):
        self.newInput("Struct List", "Struct List", "structList")
        if isBase(self.elementDataType):
            self.newOutput(toListDataType(self.elementDataType), "Data")
        else:
            self.newOutput("Generic List", "Data")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "elementKey")
        self.invokeSelector(col, "DATA_TYPE", "assignType",
            text = self.elementDataType, icon = "TRIA_RIGHT")

    def assignType(self, dataType):
        self.elementDataType = dataType

    def execute(self, structList):
        key = (self.elementDataType, self.elementKey)
        try:
            return [struct[key] for struct in structList]
        except:
            self.setErrorMessage("The key does not exist in all structs")
            return []
