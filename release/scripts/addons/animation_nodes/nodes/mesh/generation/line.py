import bpy
from .... base_types import AnimationNode
from .... algorithms.mesh_generation.line import getLineMesh

class LineMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_LineMeshNode"
    bl_label = "Line Mesh"

    def create(self):
        self.newInput("Vector", "Start", "start")
        self.newInput("Vector", "End", "end", value = [5, 0, 0])
        self.newInput("Integer", "Steps", "steps", value = 2, minValue = 2)

        self.newOutput("Mesh", "Mesh", "mesh")

    def execute(self, start, end, steps):
        steps = max(steps, 2)
        return getLineMesh(start, end, steps)