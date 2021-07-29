import bpy
from .... base_types import AnimationNode

class CylinderMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CylinderMeshNode"
    bl_label = "Cylinder Mesh"

    def create(self):
        self.newInput("Float", "Radius", "radius", value = 1, minValue = 0)
        self.newInput("Float", "Height", "height", value = 2, minValue = 0)
        self.newInput("Integer", "Resolution", "resolution", value = 8, minValue = 2)
        self.newInput("Boolean", "Caps", "caps", value = True)

        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")
        self.newOutput("Polygon Indices List", "Polygon Indices", "polygonIndices")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        yield "_resolution = max(resolution, 2)"
        yield "cylinder = animation_nodes.algorithms.mesh_generation.cylinder"
        if isLinked["vertices"]:       yield "vertices = cylinder.vertices(max(radius, 0), max(height, 0), _resolution)"
        if isLinked["edgeIndices"]:    yield "edgeIndices = cylinder.edges(_resolution)"
        if isLinked["polygonIndices"]: yield "polygonIndices = cylinder.polygons(_resolution, caps)"
