import bpy
from .... base_types import AnimationNode

class MoveObjectNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MoveObjectNode"
    bl_label = "Move Object"

    def create(self):
        self.newInput("Object", "Object", "object").defaultDrawType = "PROPERTY_ONLY"
        self.newInput("Vector", "Translation", "translation").defaultDrawType = "PROPERTY_ONLY"
        self.newOutput("Object", "Object", "object")

    def getExecutionCode(self):
        return "if object: object.location += translation"
