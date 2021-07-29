import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... events import executionCodeChanged

class RayCastBVHTreeNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_RayCastBVHTreeNode"
    bl_label = "Ray Cast BVHTree"
    bl_width_default = 150
    autoVectorizeExecution = True

    useStartList = VectorizedNode.newVectorizeProperty()
    useDirectionList = VectorizedNode.newVectorizeProperty()

    startInInfinity = BoolProperty(name = "Start in Infinity", default = False,
        description = ("Ray cast the whole line defined by location and direction."
                       " This makes the distance sockets unusable."),
        update = executionCodeChanged)

    def create(self):
        self.newInput("BVHTree", "BVHTree", "bvhTree")

        self.newVectorizedInput("Vector", "useStartList",
            ("Ray Start", "start"), ("Ray Starts", "starts"))
        self.newVectorizedInput("Vector", "useDirectionList",
            ("Ray Direction", "direction", dict(value = (0, 0, -1))),
            ("Ray Directions", "directions"))

        self.newInput("Float", "Min Distance", "minDistance", value = 0.001, hide = True)
        self.newInput("Float", "Max Distance", "maxDistance", value = 1e6, hide = True)

        useListOutput = [("useStartList", "useDirectionList")]

        self.newVectorizedOutput("Vector", useListOutput,
            ("Location", "location"), ("Locations", "locations"))
        self.newVectorizedOutput("Vector", useListOutput,
            ("Normal", "normal"), ("Normals", "normals"))
        self.newVectorizedOutput("Float", useListOutput,
            ("Distance", "distance"), ("Distances", "distances"))
        self.newVectorizedOutput("Integer", useListOutput,
            ("Polygon Index", "polygonIndex", dict(hide = True)),
            ("Polygon Indices", "polygonIndices", dict(hide = True)))
        self.newVectorizedOutput("Boolean", useListOutput,
            ("Hit", "hit"), ("Hits", "hits"))

    def draw(self, layout):
        layout.prop(self, "startInInfinity")

    def getExecutionCode(self):
        yield "_direction = direction.normalized()"
        if self.startInInfinity:
            yield from self.iterStartInInfinityCode()
        else:
            yield from self.iterStartAtLocationCode()

    def iterStartAtLocationCode(self):
        yield "location, normal, polygonIndex, distance = bvhTree.ray_cast(start + _direction * minDistance, _direction, maxDistance - minDistance)"
        yield "if location is None:"
        yield "    location = Vector((0, 0, 0))"
        yield "    polygonIndex = -1"
        yield "    normal = Vector((0, 0, 0))"
        yield "    distance = 0"
        yield "    hit = False"
        yield "else:"
        yield "    hit = True"
        yield "    distance += minDistance"

    def iterStartInInfinityCode(self):
        yield "_start = start - _direction * 100000"
        yield "distance = 0"
        yield "location, normal, polygonIndex, _ = bvhTree.ray_cast(_start, _direction)"
        yield "if location is None:"
        yield "    location = Vector((0, 0, 0))"
        yield "    polygonIndex = -1"
        yield "    normal = Vector((0, 0, 0))"
        yield "    hit = False"
        yield "else:"
        yield "    hit = True"
