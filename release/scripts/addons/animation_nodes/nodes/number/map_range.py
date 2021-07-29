import bpy
from bpy.props import *
from ... base_types import VectorizedNode

from . c_utils import (
    mapRange_DoubleList,
    mapRange_DoubleList_Interpolated
)

class MapRangeNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_MapRangeNode"
    bl_label = "Map Range"
    bl_width_default = 190

    clampInput = BoolProperty(name = "Clamp Input", default = True,
        description = "The input will be between Input Min and Input Max",
        update = VectorizedNode.refresh)

    useInterpolation = BoolProperty(name = "Use Interpolation", default = False,
        description = "Use custom interpolation between Min and Max (only available when clamp is turned on)",
        update = VectorizedNode.refresh)

    useValueList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Float", "useValueList",
            ("Value", "value"), ("Values", "values"))

        self.newInput("Float", "Input Min", "inMin", value = 0)
        self.newInput("Float", "Input Max", "inMax", value = 1)
        self.newInput("Float", "Output Min", "outMin", value = 0)
        self.newInput("Float", "Output Max", "outMax", value = 1)

        if self.useInterpolation and self.clampInput:
            self.newInput("Interpolation", "Interpolation", "interpolation", defaultDrawType = "PROPERTY_ONLY")

        self.newVectorizedOutput("Float", "useValueList",
            ("Value", "newValue"), ("Values", "newValues"))

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "clampInput", icon = "FULLSCREEN_EXIT", text = "Clamp", toggle = False)

        subrow = row.row(align = True)
        subrow.active = self.clampInput
        subrow.prop(self, "useInterpolation", icon = "IPO_BEZIER", text = "Interpolate")

    def getExecutionCode(self):
        if self.useValueList:
            if self.useInterpolation and self.clampInput:
                yield "newValues = self.execute_Multiple_Interpolated("
                yield "    values, inMin, inMax, outMin, outMax, interpolation)"
            else:
                yield "newValues = self.execute_Multiple(values, inMin, inMax, outMin, outMax)"
        else:
            yield from self.iterExecutionCode_Single()

    def iterExecutionCode_Single(self):
        yield "if inMin == inMax: newValue = 0"
        yield "else:"
        if self.clampInput:
            yield "    _value = min(max(value, inMin), inMax) if inMin < inMax else min(max(value, inMax), inMin)"
            if self.useInterpolation:
                yield "    newValue = outMin + interpolation((_value - inMin) / (inMax - inMin)) * (outMax - outMin)"
            else:
                yield "    newValue = outMin + (_value - inMin) / (inMax - inMin) * (outMax - outMin)"
        else:
            yield "    newValue = outMin + (value - inMin) / (inMax - inMin) * (outMax - outMin)"

    def execute_Multiple(self, values, inMin, inMax, outMin, outMax):
        return mapRange_DoubleList(values, self.clampInput, inMin, inMax, outMin, outMax)

    def execute_Multiple_Interpolated(self, values, inMin, inMax, outMin, outMax, interpolation):
        return mapRange_DoubleList_Interpolated(values, interpolation, inMin, inMax, outMin, outMax)
