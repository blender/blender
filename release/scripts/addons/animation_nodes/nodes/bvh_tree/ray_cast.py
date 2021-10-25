import bpy
from bpy.props import *
from ... events import executionCodeChanged
from ... base_types import AnimationNode, VectorizedSocket

class RayCastBVHTreeNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_RayCastBVHTreeNode"
    bl_label = "Ray Cast BVHTree"
    bl_width_default = 160
    codeEffects = [VectorizedSocket.CodeEffect]

    useStartList = VectorizedSocket.newProperty()
    useDirectionList = VectorizedSocket.newProperty()

    startInInfinity = BoolProperty(name = "Start in Infinity", default = False,
        description = ("Ray cast the whole line defined by location and direction."
                       " This makes the distance sockets unusable."),
        update = executionCodeChanged)

    def create(self):
        self.newInput("BVHTree", "BVHTree", "bvhTree")

        self.newInput(VectorizedSocket("Vector", "useStartList",
            ("Ray Start", "start"), ("Ray Starts", "starts")))
        self.newInput(VectorizedSocket("Vector", "useDirectionList",
            ("Ray Direction", "direction", dict(value = (0, 0, -1))),
            ("Ray Directions", "directions"),
            codeProperties = dict(default = (0, 0, -1))))

        self.newInput("Float", "Min Distance", "minDistance", value = 0.001, hide = True)
        self.newInput("Float", "Max Distance", "maxDistance", value = 1e6, hide = True)

        useListOutput = ["useStartList", "useDirectionList"]

        self.newOutput(VectorizedSocket("Vector", useListOutput,
            ("Location", "location"), ("Locations", "locations")))
        self.newOutput(VectorizedSocket("Vector", useListOutput,
            ("Normal", "normal"), ("Normals", "normals")))
        self.newOutput(VectorizedSocket("Float", useListOutput,
            ("Distance", "distance"), ("Distances", "distances")))
        self.newOutput(VectorizedSocket("Integer", useListOutput,
            ("Polygon Index", "polygonIndex", dict(hide = True)),
            ("Polygon Indices", "polygonIndices", dict(hide = True))))
        self.newOutput(VectorizedSocket("Boolean", useListOutput,
            ("Hit", "hit"), ("Hits", "hits")))

    def draw(self, layout):
        layout.prop(self, "startInInfinity")

    def getExecutionCode(self, required):
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
