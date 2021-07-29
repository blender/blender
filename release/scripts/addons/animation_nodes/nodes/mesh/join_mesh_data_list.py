import bpy
from ... data_structures import MeshData
from ... base_types import AnimationNode

class JoinMeshDataListNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_JoinMeshDataListNode"
    bl_label = "Join Mesh Data List"

    def create(self):
        self.newInput("Mesh Data List", "Mesh Data List", "meshDataList", dataIsModified = True)
        self.newOutput("Mesh Data", "Mesh Data", "meshData")

    def execute(self, meshDataList):
        return MeshData.join(*meshDataList)
