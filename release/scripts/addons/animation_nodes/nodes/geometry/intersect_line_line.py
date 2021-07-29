import bpy
from ... base_types import AnimationNode

class IntersectLineLineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectLineLineNode"
    bl_label = "Intersect Line Line"
    bl_width_default = 160
    searchTags = ["Closest Points on 2 Lines"]

    def create(self):
        self.newInput("Vector", "Line 1 Start", "lineStart1")
        self.newInput("Vector", "Line 1 End", "lineEnd1", value = [0, 1, 0])
        self.newInput("Vector", "Line 2 Start", "lineStart2")
        self.newInput("Vector", "Line 2 End", "lineEnd2", value = [0, 0, 1])

        self.newOutput("Vector", "Closest On Line 1", "closest1")
        self.newOutput("Vector", "Closest On Line 2", "closest2")
        self.newOutput("Boolean", "Is Valid", "isValid")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()): return

        yield "closestPoints = mathutils.geometry.intersect_line_line(lineStart1, lineEnd1, lineStart2, lineEnd2)"

        yield "if closestPoints is None:"
        if isLinked["closest1"]: yield "    closest1 = Vector((0, 0, 0))"
        if isLinked["closest2"]: yield "    closest2 = Vector((0, 0, 0))"
        if isLinked["isValid"]:  yield "    isValid = False"
        yield "else:"
        if isLinked["closest1"]: yield "    closest1 = closestPoints[0]"
        if isLinked["closest2"]: yield "    closest2 = closestPoints[1]"
        if isLinked["isValid"]:  yield "    isValid = True"

    def getUsedModules(self):
        return ["mathutils"]
