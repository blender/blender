import bpy
from bpy.props import *
from ... base_types import AnimationNode, AutoSelectDataType
from ... sockets.info import toIdName

class ConvertNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConvertNode"
    bl_label = "Convert"
    bl_width = 100

    dataType = StringProperty(default = "Generic", update = AnimationNode.refresh)
    lastCorrectionType = IntProperty()

    fixedOutputDataType = BoolProperty(name = "Fixed Data Type", default = False,
        description = "When activated the output type does not automatically change",
        update = AnimationNode.refresh)

    def setup(self):
        self.width_hidden = 45

    def create(self):
        self.newInput("Generic", "Old", "old", dataIsModified = True)
        self.newOutput(self.dataType, "New", "new")

        if not self.fixedOutputDataType:
            self.newSocketEffect(AutoSelectDataType(
                "dataType", [self.outputs[0]], ignore = {"Generic"}))

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

    def getExecutionCode(self):
        yield "new, self.lastCorrectionType = self.outputs[0].correctValue(old)"
