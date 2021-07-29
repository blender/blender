import bpy
from bpy.props import *
from ... base_types import AnimationNode

class ProjectPointOnLineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ProjectPointOnLineNode"
    bl_label = "Project Point on Line"
    bl_width_default = 160
    searchTags = ["Distance Point to Line", "Closest Point on Line"]

    def create(self):
        self.newInput("Vector", "Point", "point")
        self.newInput("Vector", "Line Start", "lineStart", value = (0, 0, 0))
        self.newInput("Vector", "Line End", "lineEnd", value = (0, 0, 1))

        self.newOutput("Vector", "Projection", "projection")
        self.newOutput("Float", "Projection Factor", "factor")
        self.newOutput("Float", "Distance", "distance")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()):
            return

        yield "if lineStart == lineEnd: projection, factor = lineStart, 0.0"
        yield "else: projection, factor = mathutils.geometry.intersect_point_line(point, lineStart, lineEnd)"

        if isLinked["distance"]:
            yield "distance = (projection - point).length"

    def getUsedModules(self):
        return ["mathutils"]
