import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... algorithms.mesh_generation import revolve
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

projectionTypeItems = [
    ("PARAMETER", "Same Parameter", "", "NONE", 0),
    ("PROJECT", "Project", "", "NONE", 1) ]

class RevolveSplineNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RevolveSplineNode"
    bl_label = "Revolve Spline"
    bl_width_default = 160

    projectionType = EnumProperty(name = "Projection Type", default = "PROJECT",
        items = projectionTypeItems, update = propertyChanged)

    def create(self):
        self.newInput("Spline", "Axis", "axis")
        self.newInput("Spline", "Profile", "profile")
        self.newInput("Integer", "Spline Samples", "splineSamples", value = 16, minValue = 2)
        self.newInput("Integer", "Surface Samples", "surfaceSamples", value = 16, minValue = 3)
        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def draw(self, layout):
        layout.prop(self, "projectionType", text = "")

    def execute(self, axis, profile, splineSamples, surfaceSamples):
        def canExecute():
            if not axis.isEvaluable(): return False
            if not profile.isEvaluable(): return False
            if splineSamples < 2: return False
            if surfaceSamples < 3: return False
            return True

        vertices, edgeIndices, polygonIndices = None, None, None

        if canExecute():
            if self.outputs[0].isLinked: vertices = revolve.vertices(axis, profile, splineSamples, surfaceSamples, self.projectionType)
            if self.outputs[1].isLinked: edgeIndices = revolve.edges(splineSamples, surfaceSamples)
            if self.outputs[2].isLinked: polygonIndices = revolve.polygons(splineSamples, surfaceSamples)

        if vertices is None: vertices = Vector3DList()
        if edgeIndices is None: edgeIndices = EdgeIndicesList()
        if polygonIndices is None: polygonIndices = PolygonIndicesList()

        return vertices, edgeIndices, polygonIndices
