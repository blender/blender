import bpy
from bpy.props import *
from . c_utils import separatePolygons
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, PolygonIndicesList

class SeparatePolygonsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparatePolygonsNode"
    bl_label = "Separate Polygons"

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Vector List", "Vertices", "inVertices")
        self.newInput("Polygon Indices List", "Polygon Indices", "inPolygonIndices")

        self.newOutput("Vector List", "Vertices", "outVertices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "outPolygonIndices")

    def draw(self, layout):
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def execute(self, vertices, polygons):
        self.errorMessage = ""
        if len(polygons) == 0 or polygons.getMaxIndex() < len(vertices):
            return separatePolygons(vertices, polygons)
        else:
            self.errorMessage = "Invalid polygon indices"
            return Vector3DList(), PolygonIndicesList()
