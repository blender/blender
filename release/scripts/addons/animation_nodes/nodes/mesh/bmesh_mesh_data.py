import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... data_structures import Vector3DList, EdgeIndicesList, PolygonIndicesList

class BMeshMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_BMeshMeshNode"
    bl_label = "BMesh Mesh"

    def create(self):
        self.newInput("BMesh", "BMesh", "bm")

        self.newOutput("Vector List", "Vertex Locations", "vertexLocations")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        yield "if bm:"
        if "vertexLocations" in required:
            yield "    vertexLocations = self.getVertexLocations(bm)"
        if "edgeIndices" in required:
            yield "    edgeIndices = self.getEdgeIndices(bm)"
        if "polygonIndices" in required:
            yield "    polygonIndices = self.getPolygonIndices(bm)"

        yield "else:"
        yield "    vertexLocations = Vector3DList()"
        yield "    edgeIndices = EdgeIndicesList()"
        yield "    polygonIndices = PolygonIndicesList()"

    def getVertexLocations(self, bMesh):
        return Vector3DList.fromValues(v.co for v in bMesh.verts)

    def getEdgeIndices(self, bMesh):
        return EdgeIndicesList.fromValues(tuple(v.index for v in edge.verts) for edge in bMesh.edges)

    def getPolygonIndices(self, bMesh):
        return PolygonIndicesList.fromValues(tuple(v.index for v in face.verts) for face in bMesh.faces)
