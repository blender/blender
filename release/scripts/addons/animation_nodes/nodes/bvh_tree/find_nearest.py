import bpy
from ... base_types import VectorizedNode

class FindNearestSurfacePointNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_FindNearestSurfacePointNode"
    bl_label = "Find Nearest Surface Point"
    bl_width_default = 165
    autoVectorizeExecution = True

    useVectorList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newInput("BVHTree", "BVHTree", "bvhTree")

        self.newVectorizedInput("Vector", "useVectorList",
            ("Vector", "vector"), ("Vectors", "vectors"))

        self.newInput("Float", "Max Distance", "maxDistance",
                      minValue = 0, value = 1e6, hide = True)

        self.newVectorizedOutput("Vector", "useVectorList",
            ("Location", "location"), ("Locations", "locations"))

        self.newVectorizedOutput("Vector", "useVectorList",
            ("Normal", "normal"), ("Normals", "normals"))

        self.newVectorizedOutput("Float", "useVectorList",
            ("Distance", "distance"), ("Distances", "distances"))

        self.newVectorizedOutput("Integer", "useVectorList",
            ("Polygon Index", "polygonIndex", dict(hide = True)),
            ("Polygon Indices", "polygonIndices", dict(hide = True)))

        self.newVectorizedOutput("Boolean", "useVectorList",
            ("Hit", "hit"), ("Hits", "hits"))

    def getExecutionCode(self):
        yield "location, normal, polygonIndex, distance = bvhTree.find_nearest(vector, maxDistance)"
        yield "if location is None:"
        yield "    location = Vector((0, 0, 0))"
        yield "    normal = Vector((0, 0, 0))"
        yield "    polygonIndex = -1"
        yield "    distance = 0"
        yield "    hit = False"
        yield "else: hit = True"
