import bpy
from ... base_types import AnimationNode

class FindPointsInRadiusInKDTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FindPointsInRadiusInKDTreeNode"
    bl_label = "Find Points in Radius"

    def create(self):
        self.newInput("KDTree", "KDTree", "kdTree")
        self.newInput("Float", "Radius", "radius", value = 5, minValue = 0)
        self.newInput("Vector", "Vector", "searchVector", defaultDrawType = "PROPERTY_ONLY")

        self.newOutput("an_VectorListSocket", "Vectors", "nearestVectors")
        self.newOutput("an_FloatListSocket", "Distances", "distances")
        self.newOutput("an_IntegerListSocket", "Indices", "indices")

    def getExecutionCode(self):
        yield "nearestVectors = Vector3DList()"
        yield "distances = DoubleList()"
        yield "indices = LongList()"
        yield "for vector, index, distance in kdTree.find_range(searchVector, max(radius, 0)):"
        yield "    nearestVectors.append(vector)"
        yield "    indices.append(index)"
        yield "    distances.append(distance)"
