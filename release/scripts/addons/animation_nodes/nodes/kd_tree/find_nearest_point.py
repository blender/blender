import bpy
from ... base_types import AnimationNode, VectorizedSocket

class FindNearestPointInKDTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FindNearestPointInKDTreeNode"
    bl_label = "Find Nearest Point"
    codeEffects = [VectorizedSocket.CodeEffect]

    useVectorList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("KDTree", "KDTree", "kdTree")
        self.newInput(VectorizedSocket("Vector", "useVectorList",
            ("Vector", "searchVector", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Vectors", "searchVectors")))

        self.newOutput(VectorizedSocket("Vector", "useVectorList",
            ("Vector", "nearestVector"), ("Vectors", "nearestVectors")))
        self.newOutput(VectorizedSocket("Float", "useVectorList",
            ("Distance", "distance"), ("Distances", "distances")))
        self.newOutput(VectorizedSocket("Integer", "useVectorList",
            ("Index", "index"), ("Indices", "indices")))

    def getExecutionCode(self, required):
        yield "nearestVector, index, distance = kdTree.find(searchVector)"
        yield "if nearestVector is None:"
        yield "    nearestVector, index, distance = Vector((0, 0, 0)), 0.0, -1"
