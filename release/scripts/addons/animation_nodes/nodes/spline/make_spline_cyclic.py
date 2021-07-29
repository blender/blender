import bpy
from ... base_types import AnimationNode

class MakeSplineCyclicNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MakeSplineCyclicNode"
    bl_label = "Make Spline Cyclic"

    def create(self):
        socket = self.newInput("Spline", "Spline", "spline")
        socket.dataIsModified = True
        socket.defaultDrawType = "PROPERTY_ONLY"
        self.newInput("Boolean", "Cyclic", "cylic", value = True)
        self.newOutput("Spline", "Spline", "outSpline")

    def execute(self, spline, cyclic):
        spline.cyclic = cyclic
        spline.markChanged()
        return spline
