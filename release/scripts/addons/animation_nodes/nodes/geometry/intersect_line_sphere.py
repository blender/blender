import bpy
from mathutils import Vector
from ... base_types import AnimationNode, VectorizedSocket
from ... data_structures import VirtualVector3DList, VirtualDoubleList
from . c_utils import intersect_LineSphere_List, intersect_LineSphere_Single

class IntersectLineSphereNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectLineSphereNode"
    bl_label = "Intersect Line Sphere"
    bl_width_default = 200

    useLineStartList = VectorizedSocket.newProperty()
    useLineEndList = VectorizedSocket.newProperty()
    useSphereCenterList = VectorizedSocket.newProperty()
    useSphereRadiusList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useLineStartList",
            ("Line Start", "lineStart", dict(value = (0, 0, 0))),
            ("Line Start", "lineStarts"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "useLineEndList",
            ("Line End", "lineEnd", dict(value = (0, 0, 2))),
            ("Line Ends", "lineEnds"),
            codeProperties = dict(default = (0, 0, 2))))

        self.newInput(VectorizedSocket("Vector", "useSphereCenterList",
            ("Sphere Center", "sphereCenter", dict(value = (0, 0, 0))),
            ("Sphere Centers", "sphereCenters"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Float", "useSphereRadiusList",
            ("Sphere Radius", "sphereRadius", dict(value = 1)),
            ("Spheres Radii", "sphereRadii"),
            codeProperties = dict(default = 1)))

        props = ["useLineStartList", "useLineEndList",
                 "useSphereCenterList", "useSphereRadiusList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("First Intersection", "firstIntersection"),
            ("First Intersections", "firstIntersections")))
        self.newOutput(VectorizedSocket("Vector", props,
            ("Second Intersection", "secondIntersection"),
            ("Second Intersections", "secondIntersections")))

        self.newOutput(VectorizedSocket("Float", props,
            ("First Parameter", "firstParameter"),
            ("First Parameters", "firstParameters")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Second Parameter", "secondParameter"),
            ("Second Parameters", "secondParameters")))

        self.newOutput(VectorizedSocket("Integer", props,
            ("Number Of Intersections", "numberOfIntersections"),
            ("Number Of Intersections", "numberOfIntersections")))

    def getExecutionFunctionName(self):
        useList = any((self.useLineStartList, self.useLineEndList,
                       self.useSphereCenterList, self.useSphereRadiusList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, lineStarts, lineEnds, sphereCenters, sphereRadii):
        lineStarts = VirtualVector3DList.create(lineStarts, Vector((0, 0, 0)))
        lineEnds = VirtualVector3DList.create(lineEnds, Vector((0, 0, 2)))
        sphereCenters = VirtualVector3DList.create(sphereCenters, Vector((0, 0, 0)))
        sphereRadii = VirtualDoubleList.create(sphereRadii, 1)
        amount = VirtualVector3DList.getMaxRealLength(lineStarts, lineEnds,
                                                      sphereCenters, sphereRadii)
        return intersect_LineSphere_List(amount, lineStarts, lineEnds, sphereCenters, sphereRadii)

    def execute_Single(self, lineStart, lineEnd, sphereCenter, sphereRadius):
        return intersect_LineSphere_Single(lineStart, lineEnd, sphereCenter, sphereRadius)
