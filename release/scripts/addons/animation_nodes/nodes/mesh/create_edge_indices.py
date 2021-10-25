import bpy
from . c_utils import createEdgeIndices
from ... data_structures import VirtualLongList
from ... base_types import AnimationNode, VectorizedSocket

class CreateEdgeIndicesNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CreateEdgeIndicesNode"
    bl_label = "Create Edge Indices"

    useList1 = VectorizedSocket.newProperty()
    useList2 = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Integer", "useList1",
            ("Index 1", "index1", dict(value = 0, minValue = 0)),
            ("Indices 1", "indices1")))

        self.newInput(VectorizedSocket("Integer", "useList2",
            ("Index 2", "index2", dict(value = 1, minValue = 0)),
            ("Indices 2", "indices2")))

        self.newOutput(VectorizedSocket("Edge Indices", ["useList1", "useList2"],
            ("Edge Indices", "edgeIndices"),
            ("Edge Indices List", "edgeIndicesList")))

    def getExecutionFunctionName(self):
        if self.useList1 or self.useList2:
            return "execute_List"

    def getExecutionCode(self, required):
        return "edgeIndices = (max(index1, 0), max(index2, 0))"

    def execute_List(self, indices1, indices2):
        _indices1 = VirtualLongList.create(indices1, 0)
        _indices2 = VirtualLongList.create(indices2, 0)
        amount = VirtualLongList.getMaxRealLength(_indices1, _indices2)
        return createEdgeIndices(amount, _indices1, _indices2)
