import bpy
from ... base_types import AnimationNode
from ... data_structures.splines.connect import connectSplines

class ConnectSplinesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ConnectSplinesNode"
    bl_label = "Connect Splines"

    def create(self):
        self.newInput("Spline List", "Splines", "splines", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Spline", "Spline", "spline")

    def execute(self, splines):
        return connectSplines(splines)
