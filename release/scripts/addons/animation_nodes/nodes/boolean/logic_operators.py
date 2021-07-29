import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... events import executionCodeChanged

operationItems = [
    ("AND", "A and B", "", "NONE", 0),
    ("NAND", "not (A and B)", "", "NONE", 1),
    ("OR", "A or B", "", "NONE", 2),
    ("NOR", "not (A or B)", "", "NONE", 3),
    ("XOR", "A xor B", "A must be different than B", "NONE", 4) ]

operationLabels = {item[0] : item[1] for item in operationItems}

class LogicOperatorsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LogicOperatorsNode"
    bl_label = "Logic Operators"
    dynamicLabelType = "HIDDEN_ONLY"

    operation = EnumProperty(name = "Operation", default = "AND",
        items = operationItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Boolean", "A", "a")
        self.newInput("Boolean", "B", "b")
        self.newOutput("Boolean", "Result", "result")

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def drawLabel(self):
        return operationLabels[self.operation]

    def getExecutionCode(self):
        op = self.operation
        if op == "AND":  return "result = a and b"
        if op == "NAND": return "result = not (a and b)"
        if op == "OR":   return "result = a or b"
        if op == "NOR":  return "result = not (a or b)"
        if op == "XOR":  return "result = a ^ b"
