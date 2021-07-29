import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode

from ... algorithms.interpolations import (
    MirroredInterpolation,
    MirroredAndChainedInterpolation
)

class MirrorInterpolationNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MirrorInterpolationNode"
    bl_label = "Mirror Interpolation"

    chain = BoolProperty(name = "Chain", default = True,
        description = "Connect original and mirrored interpolation.",
        update = propertyChanged)

    def create(self):
        self.newInput("Interpolation", "Interpolation", "interpolationIn", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Interpolation", "Interpolation", "interpolationOut")

    def draw(self, layout):
        layout.prop(self, "chain")

    def execute(self, interpolation):
        if self.chain:
            return MirroredAndChainedInterpolation(interpolation)
        else:
            return MirroredInterpolation(interpolation)
