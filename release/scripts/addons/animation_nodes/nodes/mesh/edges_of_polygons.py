import bpy
from ... base_types import AnimationNode
from ... data_structures import EdgeIndicesList
from ... data_structures.meshes.validate import createValidEdgesList

class EdgesOfPolygonsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EdgesOfPolygonsNode"
    bl_label = "Edges of Polygons"

    def create(self):
        self.newInput("Polygon Indices List", "Polygons", "polygons")
        self.newOutput("Edge Indices List", "Edges", "edges")

    def execute(self, polygons):
        return createValidEdgesList(polygons = polygons)
