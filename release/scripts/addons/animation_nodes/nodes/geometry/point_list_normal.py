import bpy
from bpy.props import *
from ... utils.layout import writeText
from ... base_types import AnimationNode

class PointListNormalNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_PointListNormalNode"
    bl_label = "Point List Normal"
    bl_width_default = 160

    searchTags = ["Points Normal", "Calculate Normal"]
    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Point List", "points")
        self.newOutput("Vector", "Normal", "normal")
        self.newOutput("Boolean", "Is Valid", "isValid")

    def draw(self, layout):
        if self.errorMessage != "":
            writeText(layout, self.errorMessage, icon = "ERROR", width = 20)

    def getExecutionCode(self):
        yield "if len(points) >= 3:"
        yield "    normal = mathutils.geometry.normal(points)"
        yield "    self.errorMessage =  '' "
        yield "else:"
        yield "    normal = Vector((0, 0, 0))"
        yield "    self.errorMessage =  'Expected min 3 different vectors' "

        yield "isValid = normal[:] != (0, 0, 0)"

    def getUsedModules(self):
        return ["mathutils"]
