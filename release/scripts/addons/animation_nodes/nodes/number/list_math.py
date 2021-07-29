import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

operationItems = [
    ("ADD", "Add", "", "", 0),
    ("MULTIPLY", "Multiply", "", "", 1),
    ("MIN", "Min", "", "", 2),
    ("MAX", "Max", "", "", 3),
    ("AVERAGE", "Average", "", "", 4) ]

class NumberListMathNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_NumberListMathNode"
    bl_label = "Number List Math"

    operation = EnumProperty(name = "Operation", default = "ADD",
        items = operationItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Float List", "Number List", "numbers")
        self.newOutput("Float", "Result", "result")

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def getExecutionCode(self):
        if self.operation == "ADD":
            yield "result = numbers.getSumOfElements()"
        elif self.operation == "MULTIPLY":
            yield "result = numbers.getProductOfElements()"
        elif self.operation == "MIN":
            yield "result = numbers.getMinValue() if len(numbers) > 0 else 0"
        elif self.operation == "MAX":
            yield "result = numbers.getMaxValue() if len(numbers) > 0 else 0"
        elif self.operation == "AVERAGE":
            yield "result = numbers.getAverageOfElements() if len(numbers) > 0 else 0"
