import bpy
from bpy.props import *
from ... base_types import AnimationNode, AutoSelectDataType

compare_types = ["A = B", "A != B", "A < B", "A <= B", "A > B", "A >= B", "A is B","A is None"]
compare_types_items = [(t, t, "") for t in compare_types]

numericLabelTypes = ["Integer", "Float"]

class CompareNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CompareNode"
    bl_label = "Compare"
    dynamicLabelType = "HIDDEN_ONLY"

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Integer")

    compareType = EnumProperty(name = "Compare Type",
        items = compare_types_items, update = AnimationNode.refresh)

    def create(self):
        self.newInput(self.assignedType, "A", "a")
        if self.compareType != "A is None":
            self.newInput(self.assignedType, "B", "b")
        self.newOutput("an_BooleanSocket", "Result", "result")

        self.newSocketEffect(AutoSelectDataType("assignedType",
            [self.inputs[0],
             self.inputs[1] if len(self.inputs) == 2 else None]
        ))

    def draw(self, layout):
        layout.prop(self, "compareType", text = "Type")

    def drawLabel(self):
        label = self.compareType
        if self.assignedType in numericLabelTypes:
            if getattr(self.socketA, "isUnlinked", False):
                label = label.replace("A", str(round(self.socketA.value, 4)))
            if getattr(self.socketB, "isUnlinked", False):
                label = label.replace("B", str(round(self.socketB.value, 4)))
        return label

    def drawAdvanced(self, layout):
        self.invokeSelector(layout, "DATA_TYPE", "assignType",
            text = "Change Type", icon = "TRIA_RIGHT")

    def getExecutionCode(self):
        type = self.compareType
        if type == "A = B":     return "result = a == b"
        if type == "A != B":    return "result = a != b"
        if type == "A < B":	    return "try: result = a < b \nexcept: result = False"
        if type == "A <= B":    return "try: result = a <= b \nexcept: result = False"
        if type == "A > B":	    return "try: result = a > b \nexcept: result = False"
        if type == "A >= B":    return "try: result = a >= b \nexcept: result = False"
        if type == "A is B":    return "result = a is b"
        if type == "A is None": return "result = a is None"
        return "result = False"

    def assignType(self, dataType):
        if self.assignedType == dataType: return
        self.assignedType = dataType

    @property
    def socketA(self):
        return self.inputs.get("A")

    @property
    def socketB(self):
        return self.inputs.get("B")
