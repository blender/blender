import bpy
from mathutils import Vector
from ... data_structures import VirtualVector3DList
from ... base_types import AnimationNode, VectorizedSocket
from . c_utils import intersect_LinePlane_List, intersect_LinePlane_Single

class IntersectLinePlaneNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_IntersectLinePlaneNode"
    bl_label = "Intersect Line Plane"
    bl_width_default = 160

    useLineStartList = VectorizedSocket.newProperty()
    useLineEndList = VectorizedSocket.newProperty()
    usePlaneNormalList = VectorizedSocket.newProperty()
    usePlanePointList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Vector", "useLineStartList",
            ("Line Start", "lineStart", dict(value = (0, 0, 1))),
            ("Line Starts", "lineStarts"),
            codeProperties = dict(default = (0, 0, 1))))
        self.newInput(VectorizedSocket("Vector", "useLineEndList",
            ("Line End", "lineEnd", dict(value = (0, 0, -1))),
            ("Line Ends", "lineEnds"),
            codeProperties = dict(default = (0, 0, -1))))

        self.newInput(VectorizedSocket("Vector", "usePlanePointList",
            ("Plane Point", "planePoint", dict(value = (0, 0, 0))),
            ("Plane Points", "planePoints"),
            codeProperties = dict(default = (0, 0, 0))))
        self.newInput(VectorizedSocket("Vector", "usePlaneNormalList",
            ("Plane Normal", "planeNormal", dict(value = (0, 0, 1))),
            ("Plane Normals", "planeNormals"),
            codeProperties = dict(default = (0, 0, 1))))

        props = ["useLineStartList", "useLineEndList",
                 "usePlaneNormalList", "usePlanePointList"]

        self.newOutput(VectorizedSocket("Vector", props,
            ("Intersection", "intersection"),
            ("Intersections", "intersections")))
        self.newOutput(VectorizedSocket("Float", props,
            ("Parameter", "parameter"),
            ("Parameters", "parameters")))
        self.newOutput(VectorizedSocket("Boolean", props,
            ("Valid", "valid"),
            ("Valids", "valids")))

    def getExecutionFunctionName(self):
        useList = any((self.useLineStartList, self.useLineEndList,
                       self.usePlaneNormalList, self.usePlanePointList))
        if useList:
            return "execute_List"
        else:
            return "execute_Single"

    def execute_List(self, lineStarts, lineEnds, planePoints, planeNormals):
        lineStarts = VirtualVector3DList.create(lineStarts, Vector((0, 0, 1)))
        lineEnds = VirtualVector3DList.create(lineEnds, Vector((0, 0, -1)))
        planePoints = VirtualVector3DList.create(planePoints, Vector((0, 0, 0)))
        planeNormals = VirtualVector3DList.create(planeNormals, Vector((0, 0, 1)))
        amount = VirtualVector3DList.getMaxRealLength(lineStarts, lineEnds,
                                                      planePoints, planeNormals)
        return intersect_LinePlane_List(amount, lineStarts, lineEnds, planePoints, planeNormals)

    def execute_Single(self, lineStart, lineEnd, planePoint, planeNormal):
        return intersect_LinePlane_Single(lineStart, lineEnd, planePoint, planeNormal)
