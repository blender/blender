import bpy
from . c_utils import createEdgeIndices
from ... base_types import VectorizedNode

class CreateEdgeIndicesNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_CreateEdgeIndicesNode"
    bl_label = "Create Edge Indices"

    useList1 = VectorizedNode.newVectorizeProperty()
    useList2 = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Integer", "useList1",
            ("Index 1", "index1", dict(value = 0, minValue = 0)),
            ("Indices 1", "indices1"))

        self.newVectorizedInput("Integer", "useList2",
            ("Index 2", "index2", dict(value = 1, minValue = 0)),
            ("Indices 2", "indices2"))

        self.newVectorizedOutput("Edge Indices", [("useList1", "useList2")],
            ("Edge Indices", "edgeIndices"),
            ("Edge Indices List", "edgeIndicesList"))

    def getExecutionFunctionName(self):
        if self.useList1 or self.useList2:
            return "execute_List"

    def getExecutionCode(self):
        return "edgeIndices = (max(index1, 0), max(index2, 0))"

    def execute_List(self, indices1, indices2):
        return createEdgeIndices(indices1, indices2)
