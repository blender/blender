import bpy
from ... base_types import AnimationNode, VectorizedSocket

class FindNearestSurfacePointNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_FindNearestSurfacePointNode"
    bl_label = "Find Nearest Surface Point"
    bl_width_default = 160
    codeEffects = [VectorizedSocket.CodeEffect]

    useVectorList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput("BVHTree", "BVHTree", "bvhTree")

        self.newInput(VectorizedSocket("Vector", "useVectorList",
            ("Vector", "vector"), ("Vectors", "vectors")))

        self.newInput("Float", "Max Distance", "maxDistance",
                      minValue = 0, value = 1e6, hide = True)

        self.newOutput(VectorizedSocket("Vector", "useVectorList",
            ("Location", "location"), ("Locations", "locations")))

        self.newOutput(VectorizedSocket("Vector", "useVectorList",
            ("Normal", "normal"), ("Normals", "normals")))

        self.newOutput(VectorizedSocket("Float", "useVectorList",
            ("Distance", "distance"), ("Distances", "distances")))

        self.newOutput(VectorizedSocket("Integer", "useVectorList",
            ("Polygon Index", "polygonIndex", dict(hide = True)),
            ("Polygon Indices", "polygonIndices", dict(hide = True))))

        self.newOutput(VectorizedSocket("Boolean", "useVectorList",
            ("Hit", "hit"), ("Hits", "hits")))

    def getExecutionCode(self, required):
        yield "location, normal, polygonIndex, distance = bvhTree.find_nearest(vector, maxDistance)"
        yield "if location is None:"
        yield "    location = Vector((0, 0, 0))"
        yield "    normal = Vector((0, 0, 0))"
        yield "    polygonIndex = -1"
        yield "    distance = 0"
        yield "    hit = False"
        yield "else: hit = True"
