import bpy
from bpy.props import *
from ... base_types import AnimationNode, AutoSelectDataType

class SwitchNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SwitchNode"
    bl_label = "Switch"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Float")

    def create(self):
        self.newInput("an_BooleanSocket", "Condition", "condition")
        self.newInput(self.assignedType, "If True", "ifTrue")
        self.newInput(self.assignedType, "If False", "ifFalse")
        self.newOutput(self.assignedType, "Output", "output")
        self.newOutput(self.assignedType, "Other", "other", hide = True)

        self.newSocketEffect(AutoSelectDataType("assignedType",
            [self.inputs[1], self.inputs[2],
             self.outputs[0], self.outputs[1]]
        ))

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignType",
            text = "Change Type", icon = "TRIA_RIGHT")

    def assignType(self, dataType):
        if dataType == self.assignedType: return
        self.assignedType = dataType

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if isLinked["output"]: yield "output = ifTrue if condition else ifFalse"
        if isLinked["other"]:  yield "other = ifFalse if condition else ifTrue"
