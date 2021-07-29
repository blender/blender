import bpy
from .... base_types import AnimationNode

class ResetObjectTransformsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ResetObjectTransformsNode"
    bl_label = "Reset Object Transforms"

    def create(self):
        self.newInput("Object", "Object", "object").defaultDrawType = "PROPERTY_ONLY"
        self.newOutput("Object", "Object", "object")

    def getExecutionCode(self):
        return "if object: object.matrix_world = mathutils.Matrix.Identity(4)"

    def getUsedModules(self):
        return ["mathutils"]
