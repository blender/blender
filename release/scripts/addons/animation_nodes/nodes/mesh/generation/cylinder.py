import bpy
from .... base_types import AnimationNode
from .... algorithms.mesh_generation.cylinder import getCylinderMesh

class CylinderMeshNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CylinderMeshNode"
    bl_label = "Cylinder Mesh"

    def create(self):
        self.newInput("Float", "Radius", "radius", value = 1, minValue = 0)
        self.newInput("Float", "Height", "height", value = 2, minValue = 0)
        self.newInput("Integer", "Resolution", "resolution", value = 8, minValue = 2)
        self.newInput("Boolean", "Caps", "caps", value = True)

        self.newOutput("Mesh", "Mesh", "mesh")

    def execute(self, radius, height, resolution, caps):
        resolution = max(resolution, 2)
        radius = max(radius, 0)
        height = max(height, 0)
        return getCylinderMesh(radius, height, resolution, caps)