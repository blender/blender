import bpy
from ... base_types import VectorizedNode

class FindNearestPointInKDTreeNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_FindNearestPointInKDTreeNode"
    bl_label = "Find Nearest Point"
    autoVectorizeExecution = True

    useVectorList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newInput("KDTree", "KDTree", "kdTree")
        self.newVectorizedInput("Vector", "useVectorList",
            ("Vector", "searchVector", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Vectors", "searchVectors"))

        self.newVectorizedOutput("Vector", "useVectorList",
            ("Vector", "nearestVector"), ("Vectors", "nearestVectors"))
        self.newVectorizedOutput("Float", "useVectorList",
            ("Distance", "distance"), ("Distances", "distances"))
        self.newVectorizedOutput("Integer", "useVectorList",
            ("Index", "index"), ("Indices", "indices"))

    def getExecutionCode(self):
        yield "nearestVector, index, distance = kdTree.find(searchVector)"
        yield "if nearestVector is None:"
        yield "    nearestVector, index, distance = Vector((0, 0, 0)), 0.0, -1"
