import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import Matrix4x4List
from . c_utils import extractPolygonTransforms

class ExtractPolygonTransformsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ExtractPolygonTransformsNode"
    bl_label = "Extract Polygon Transforms"

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "vertices")
        self.newInput("Polygon Indices List", "Polygon Indices", "polygonIndices")

        self.newOutput("Matrix List", "Transforms", "transforms")
        self.newOutput("Matrix List", "Inverted Transforms", "invertedTransforms")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, vertices, polygons):
        self.errorMessage = ""
        if len(polygons) == 0 or polygons.getMaxIndex() < len(vertices):
            return extractPolygonTransforms(vertices, polygons, calcInverted = True)
        else:
            self.errorMessage = "Invalid polygon indices"
            return Matrix4x4List(), Matrix4x4List()
