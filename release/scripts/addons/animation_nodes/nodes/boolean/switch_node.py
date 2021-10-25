import bpy
from bpy.props import *
from ... base_types import AnimationNode, DataTypeSelectorSocket

class SwitchNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SwitchNode"
    bl_label = "Switch"

    assignedType = DataTypeSelectorSocket.newProperty(default = "Float")

    def create(self):
        self.newInput("an_BooleanSocket", "Condition", "condition")

        self.newInput(DataTypeSelectorSocket("If True", "ifTrue", "assignedType"))
        self.newInput(DataTypeSelectorSocket("If False", "ifFalse", "assignedType"))

        self.newOutput(DataTypeSelectorSocket("Output", "output", "assignedType"))
        self.newOutput(DataTypeSelectorSocket("Other", "other", "assignedType"), hide = True)

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignType",
            text = "Change Type", icon = "TRIA_RIGHT")

    def assignType(self, dataType):
        if self.assignedType != dataType:
            self.assignedType = dataType
            self.refresh()

    def getExecutionCode(self, required):
        if "output" in required: yield "output = ifTrue if condition else ifFalse"
        if "other" in required:  yield "other = ifFalse if condition else ifTrue"
