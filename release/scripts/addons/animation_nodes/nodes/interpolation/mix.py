import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... algorithms.interpolations import MixedInterpolation, ChainedInterpolation

modeItems = [
    ("OVERLAY", "Overlay", "Overlay two interpolations", "NONE", 0),
    ("CHAIN", "Chain", "Chain two interpolations", "NONE", 1)
]

class MixInterpolationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MixInterpolationNode"
    bl_label = "Mix Interpolation"
    bl_width_default = 175

    mode = EnumProperty(name = "Mode", default = "OVERLAY",
        items = modeItems, update = AnimationNode.refresh)

    def create(self):
        if self.mode == "OVERLAY":
            self.newInput("Float", "Factor", "factor").setRange(0, 1)
            self.newInput("Interpolation", "Interpolation 1", "a", defaultDrawType = "PROPERTY_ONLY")
            self.newInput("Interpolation", "Interpolation 2", "b", defaultDrawType = "PROPERTY_ONLY")
        elif self.mode == "CHAIN":
            self.newInput("Interpolation", "Interpolation 1", "a", defaultDrawType = "PROPERTY_ONLY")
            self.newInput("Interpolation", "Interpolation 2", "b", defaultDrawType = "PROPERTY_ONLY")
            self.newInput("Float", "Position", "position", value = 0.5).setRange(0, 1)
            self.newInput("Float", "End 1", "endA", value = 1).setRange(0, 1)
            self.newInput("Float", "Start 2", "startB", value = 0).setRange(0, 1)
            self.newInput("Float", "Fade Width", "fadeWidth").setRange(0, 1)
        self.newOutput("Interpolation", "Interpolation", "interpolation")

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "mode", text = "")

    def getExecutionFunctionName(self):
        if self.mode == "OVERLAY":
            return "execute_Overlay"
        elif self.mode == "CHAIN":
            return "execute_Chain"

    def execute_Overlay(self, factor, a, b):
        return MixedInterpolation(factor, a, b)

    def execute_Chain(self, a, b, position, endA, startB, fadeWidth):
        return ChainedInterpolation(a, b, position, endA, startB, fadeWidth)
