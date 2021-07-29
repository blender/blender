import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode

operationItems = [
    ("ADD", "Add", "", "", 0),
    ("AVERAGE", "Average", "", "", 1) ]

class VectorListMathNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_VectorListMathNode"
    bl_label = "Vector List Math"

    operation = EnumProperty(name = "Operation", default = "ADD",
        items = operationItems, update = executionCodeChanged)

    def create(self):
        self.newInput("Vector List", "Vector List", "vectors")
        self.newOutput("Vector", "Result", "result")

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def getExecutionCode(self):
        if self.operation == "ADD":
            yield "result = vectors.getSumOfElements()"
        elif self.operation == "AVERAGE":
            yield "result = vectors.getAverageOfElements()"
