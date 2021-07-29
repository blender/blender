import bpy
from .... base_types import AnimationNode

class LineMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LineMeshNode"
    bl_label = "Line Mesh"

    def create(self):
        self.newInput("Integer", "Steps", "steps", value = 2, minValue = 2)
        self.newInput("Vector", "Start", "start")
        self.newInput("Vector", "End", "end", value = [5, 0, 0])

        self.newOutput("Vector List", "Vertices", "vertices")
        self.newOutput("Edge Indices List", "Edge Indices", "edgeIndices")

    def getExecutionCode(self):
        isLinked = self.getLinkedOutputsDict()
        yield "_steps = max(steps, 2)"
        yield "line = animation_nodes.algorithms.mesh_generation.line"
        if isLinked["vertices"]:    yield "vertices = line.vertices(start, end, steps)"
        if isLinked["edgeIndices"]: yield "edgeIndices = line.edges(steps)"
