import bpy
from ... data_structures import Mesh
from ... base_types import AnimationNode

class JoinMeshListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_JoinMeshListNode"
    bl_label = "Join Mesh List"

    def create(self):
        self.newInput("Mesh List", "Mesh List", "meshDataList", dataIsModified = True)
        self.newOutput("Mesh", "Mesh", "meshData")

    def execute(self, meshDataList):
        return Mesh.join(*meshDataList)
