import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from . c_utils import clamp_DoubleList

class FloatClampNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_FloatClampNode"
    bl_label = "Clamp"
    dynamicLabelType = "HIDDEN_ONLY"

    useValueList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Float", "useValueList",
            ("Value", "value"),
            ("Values", "values", dict(dataIsModified = True)))

        self.newInput("Float", "Min", "minValue", value = 0.0)
        self.newInput("Float", "Max", "maxValue", value = 1.0)

        self.newVectorizedOutput("Float", "useValueList",
            ("Value", "outValue"), ("Values", "outValues"))

    def getExecutionFunctionName(self):
        if self.useValueList:
            return "execute_List"

    def execute_List(self, values, minValue, maxValue):
        clamp_DoubleList(values, minValue, maxValue)
        return values

    def getExecutionCode(self):
        yield "outValue = min(max(value, minValue), maxValue)"

    def drawLabel(self):
        label = "clamp(min, max)"
        if self.minValueSocket.isUnlinked:
            label = label.replace("min", str(round(self.minValueSocket.value, 4)))
        if self.maxValueSocket.isUnlinked:
            label = label.replace("max", str(round(self.maxValueSocket.value, 4)))
        return label

    @property
    def minValueSocket(self):
        return self.inputs.get("Min")

    @property
    def maxValueSocket(self):
        return self.inputs.get("Max")
