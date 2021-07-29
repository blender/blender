import bpy
from ... base_types import AnimationNode

class SeparateQuaternionNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparateQuaternionNode"
    bl_label = "Separate Quaternion"

    def create(self):
        self.newInput("Quaternion", "Quaternion", "quaternion")
        self.newOutput("Float", "W", "w")
        self.newOutput("Float", "X", "x")
        self.newOutput("Float", "Y", "y")
        self.newOutput("Float", "Z", "z")

    def getExecutionCode(self):
        return "w, x, y, z = quaternion"
