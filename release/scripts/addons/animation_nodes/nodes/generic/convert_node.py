import bpy
from bpy.props import *
from ... base_types import AnimationNode, DataTypeSelectorSocket

class ConvertNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertNode"
    bl_label = "Convert"
    bl_width = 100

    dataType = DataTypeSelectorSocket.newProperty(default = "Generic")
    lastCorrectionType = IntProperty()

    fixedOutputDataType = BoolProperty(name = "Fixed Data Type", default = False,
        description = "When activated the output type does not automatically change",
        update = AnimationNode.refresh)

    def setup(self):
        self.width_hidden = 45

    def create(self):
        self.newInput("Generic", "Old", "old", dataIsModified = True)

        if self.fixedOutputDataType:
            self.newOutput(self.dataType, "New", "new")
        else:
            self.newOutput(DataTypeSelectorSocket("New", "new", "dataType", ignore = {"Generic"}))

    def draw(self, layout):
        row = layout.row(align = True)
        self.invokeSelector(row, "DATA_TYPE", "assignOutputType", text = "to " + self.dataType)
        icon = "LOCKED" if self.fixedOutputDataType else "UNLOCKED"
        row.prop(self, "fixedOutputDataType", icon = icon, text = "")

        if self.lastCorrectionType == 2:
            layout.label("Conversion Failed", icon = "ERROR")

    def assignOutputType(self, dataType):
        self.fixedOutputDataType = True
        if self.dataType != dataType:
            self.dataType = dataType
            self.refresh()

    def getExecutionCode(self, required):
        yield "new, self.lastCorrectionType = self.outputs[0].correctValue(old)"
