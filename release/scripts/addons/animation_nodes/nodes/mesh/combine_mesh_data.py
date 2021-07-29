import bpy
from ... data_structures import MeshData
from ... base_types import AnimationNode

class CombineMeshDataNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CombineMeshDataNode"
    bl_label = "Combine Mesh Data"

    def create(self):
        self.newInput("Vector List", "Vertex Locations", "vertexLocations", dataIsModified = True)
        self.newInput("Edge Indices List", "Edge Indices", "edgeIndices", dataIsModified = True)
        self.newInput("Polygon Indices List", "Polygon Indices", "polygonIndices", dataIsModified = True)
        self.newOutput("an_MeshDataSocket", "Mesh Data", "meshData")

    def execute(self, vertexLocations, edgeIndices, polygonIndices):
        return MeshData(vertexLocations, edgeIndices, polygonIndices)
