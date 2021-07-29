import bpy
from ... base_types import AnimationNode

class FindNearestNPointsInKDTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FindNearestNPointsInKDTreeNode"
    bl_label = "Find Nearest Points"

    def create(self):
        self.newInput("KDTree", "KDTree", "kdTree")
        self.newInput("Integer", "Amount", "amount", value = 5, minValue = 0)
        self.newInput("Vector", "Vector", "searchVector", defaultDrawType = "PROPERTY_ONLY")
        self.newOutput("Vector List", "Vectors", "nearestVectors")
        self.newOutput("Float List", "Distances", "distances")
        self.newOutput("Integer List", "Indices", "indices")

    def getExecutionCode(self):
        yield "_amount = max(amount, 0)"
        yield "nearestVectors = Vector3DList(capacity = _amount)"
        yield "distances = DoubleList(capacity = _amount)"
        yield "indices = LongList(capacity = _amount)"
        yield "for vector, index, distance in kdTree.find_n(searchVector, _amount):"
        yield "    nearestVectors.append(vector)"
        yield "    indices.append(index)"
        yield "    distances.append(distance)"
