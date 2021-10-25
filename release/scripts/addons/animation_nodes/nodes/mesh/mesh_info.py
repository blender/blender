import bpy
from ... base_types import AnimationNode

class MeshInfoNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_MeshInfoNode"
    bl_label = "Mesh Info"

    def create(self):
        self.newInput("Mesh", "Mesh", "mesh", dataIsModified = True)
        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")
        self.newOutput("Vector List", "Vertex Normals", "vertexNormals")
        self.newOutput("Vector List", "Polygon Normals", "polygonNormals")
        self.newOutput("Vector List", "Polygon Centers", "polygonCenters")
        self.newOutput("Text List", "UV Map Names", "uvMapNames")

    def getExecutionCode(self, required):
        if "vertices" in required:
            yield "vertices = mesh.vertices"
        if "edgeIndices" in required:
            yield "edgeIndices = mesh.edges"
        if "polygonIndices" in required:
            yield "polygonIndices = mesh.polygons"
        if "vertexNormals" in required:
            yield "vertexNormals = mesh.getVertexNormals()"
        if "polygonNormals" in required:
            yield "polygonNormals = mesh.getPolygonNormals()"
        if "polygonCenters" in required:
            yield "polygonCenters = mesh.getPolygonCenters()"
        if "uvMapNames" in required:
            yield "uvMapNames = mesh.getUVMapNames()"
