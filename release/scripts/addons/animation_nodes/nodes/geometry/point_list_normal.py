import bpy
from ... base_types import AnimationNode

class PointListNormalNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_PointListNormalNode"
    bl_label = "Point List Normal"
    bl_width_default = 160
    errorHandlingType = "MESSAGE"
    searchTags = ["Points Normal", "Calculate Normal"]

    def create(self):
        self.newInput("Vector List", "Point List", "points")
        self.newOutput("Vector", "Normal", "normal")
        self.newOutput("Boolean", "Is Valid", "isValid")

    def getExecutionCode(self, required):
        yield "if len(points) >= 3:"
        yield "    normal = mathutils.geometry.normal(points)"
        yield "else:"
        yield "    normal = Vector((0, 0, 0))"
        yield "    self.setErrorMessage('Expected min 3 different vectors')"

        yield "isValid = normal[:] != (0, 0, 0)"

    def getUsedModules(self):
        return ["mathutils"]
