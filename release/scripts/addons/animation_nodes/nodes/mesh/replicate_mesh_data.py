import bpy
from bpy.props import *
from ... base_types import AnimationNode
from . c_utils import replicateMeshData

transformationTypeItems = [
    ("Matrix List", "Matrices", "", "NONE", 0),
    ("Vector List", "Vectors", "", "NONE", 1)
]

class ReplicateMeshDataNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ReplicateMeshDataNode"
    bl_label = "Replicate Mesh Data"

    transformationType = EnumProperty(name = "Transformation Type", default = "Matrix List",
        items = transformationTypeItems, update = AnimationNode.refresh)

    def create(self):
        self.newInput("Mesh Data", "Mesh Data", "sourceMeshData")
        self.newInput(self.transformationType, "Transformations", "transformations")
        self.newOutput("Mesh Data", "Mesh Data", "outMeshData")

    def draw(self, layout):
        layout.prop(self, "transformationType", text = "")

    def execute(self, source, transformations):
        return replicateMeshData(source, transformations)
