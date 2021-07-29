import bpy
from bpy.props import *
from ... base_types import AnimationNode

class ProjectPointOnPlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ProjectPointOnPlaneNode"
    bl_label = "Project Point on Plane"
    bl_width_default = 170
    searchTags = ["Distance Point to Plane", "Closest Point on Plane"]

    def create(self):
        self.newInput("Vector", "Point", "point")
        self.newInput("Vector", "Plane Point", "planePoint", value = (0, 0, 0))
        self.newInput("Vector", "Plane Normal", "planeNormal", value = (0, 0, 1))

        self.newOutput("Vector", "Projection", "projection")
        self.newOutput("Float", "Signed Distance", "distance")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        if not any(isLinked.values()):
            return

        yield "plane_co = planePoint"
        yield "plane_no = planeNormal if planeNormal.length_squared != 0 else Vector((0, 0, 1))"

        if isLinked["projection"]:
            yield "intersection = mathutils.geometry.intersect_line_plane(point, point + plane_no, plane_co, plane_no, False)"
            yield "projection = Vector((0, 0, 0)) if intersection is None else intersection"
        if isLinked["distance"]:
            yield "distance = mathutils.geometry.distance_point_to_plane(point, plane_co, plane_no)"

    def getUsedModules(self):
        return ["mathutils"]
