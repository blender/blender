import bpy
from bpy.props import *
from ... base_types import AnimationNode
from . c_utils import transformPolygons

class TransformPolygonsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_TransformPolygonsNode"
    bl_label = "Transform Polygons"

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "vertices", dataIsModified = True)
        self.newInput("Polygon Indices List", "Polygon Indices", "polygonIndices")
        self.newInput("Matrix List", "Matrices", "matrices")

        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, vertices, polygons, matrices):
        self.errorMessage = ""
        if len(polygons) != 0 and polygons.getMaxIndex() >= len(vertices):
            self.errorMessage = "Invalid polygon indices"
            return vertices, polygons
        if len(polygons) != len(matrices):
            self.errorMessage = "Different amount of polygons and matrices"
            return vertices, polygons

        transformPolygons(vertices, polygons, matrices)
        return vertices, polygons
