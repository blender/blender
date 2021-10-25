import bpy
from . c_utils import separatePolygons
from ... base_types import AnimationNode

class SeparatePolygonsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_SeparatePolygonsNode"
    bl_label = "Separate Polygons"
    errorHandlingType = "EXCEPTION"

    def create(self):
        self.newInput("Vector List", "Vertices", "inVertices")
        self.newInput("Polygon Indices List", "Polygon Indices", "inPolygonIndices")

        self.newOutput("Vector List", "Vertices", "outVertices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "outPolygonIndices")

    def execute(self, vertices, polygons):
        if len(polygons) == 0 or polygons.getMaxIndex() < len(vertices):
            return separatePolygons(vertices, polygons)
        else:
            self.raiseErrorMessage("Invalid polygon indices")
