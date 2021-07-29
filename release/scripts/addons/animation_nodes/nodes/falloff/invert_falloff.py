import bpy
from . remap_falloff import RemapFalloff
from ... base_types import AnimationNode

class InvertFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_InvertFalloffNode"
    bl_label = "Invert Falloff"

    def create(self):
        self.newInput("Falloff", "Falloff", "inFalloff")
        self.newOutput("Falloff", "Falloff", "outFalloff")

    def execute(self, falloff):
        return InvertFalloff(falloff)

class InvertFalloff:
    def __new__(cls, falloff):
        return RemapFalloff(falloff, 1, 0)
